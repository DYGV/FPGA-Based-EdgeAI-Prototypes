/*
 * Copyright 2023 Eisuke Okazaki
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <boost/json/src.hpp>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <queue>
#include <thread>
#include <vector>

#define CV_TIMEOUT 5000
#define SLEEP_SEND_FRAME 300

using namespace boost::asio;

struct FrameInfo {
    FrameInfo(cv::Mat img, ip::tcp::socket sock, std::string video_file)
        : image_in(std::queue<cv::Mat>()), socket(std::move(sock)) {
        cap.open(video_file);
        if (!cap.isOpened()) {
            exit(1);
        }

        std::string camera_device = "/dev/video";
        if (std::equal(camera_device.begin(), camera_device.end(),
                       video_file.begin())) {
            frame_count = 0;
        } else {
            frame_count = cap.get(cv::CAP_PROP_FRAME_COUNT);
        }
    }

    std::queue<cv::Mat> image_in;
    std::queue<boost::json::value> result;
    std::queue<cv::Mat> image_in_;
    std::mutex mtx_in;
    std::mutex mtx_in_;
    std::mutex mtx_result;
    std::condition_variable cv_in;
    std::condition_variable cv_in_;
    std::condition_variable cv_result;
    cv::VideoCapture cap;
    ip::tcp::socket socket;
    size_t frame_count;
};

void send_frame(FrameInfo *data) {
    size_t recv_count = 0;
    while (++recv_count != data->frame_count) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(SLEEP_SEND_FRAME));
        std::unique_lock<std::mutex> lock_in(data->mtx_in);
        bool available =
            data->cv_in.wait_for(lock_in, std::chrono::milliseconds(CV_TIMEOUT),
                                 [&data] { return !data->image_in.empty(); });
        if (!available) {
            std::cout << "Image data did not reach the queue and timed out"
                      << std::endl;
            break;
        }
        cv::Mat frame = data->image_in.front();
        data->image_in.pop();
        lock_in.unlock();
        data->cv_in.notify_one();
        std::size_t frame_size = frame.total() * frame.elemSize();
        boost::asio::write(data->socket,
                           boost::asio::buffer(frame.data, frame_size));
    }
}

void recv_result(FrameInfo *data) {
    size_t recv_count = 0;
    while (++recv_count != data->frame_count) {
        size_t result_size;
        boost::asio::read(data->socket, boost::asio::buffer(
                                            &result_size, sizeof(std::size_t)));
        std::string result_data(result_size, '\0');
        size_t len = boost::asio::read(
            data->socket, boost::asio::buffer(&result_data[0], result_size));
        boost::json::value result_json = boost::json::parse(result_data);
        std::unique_lock<std::mutex> lock_result(data->mtx_result);
        data->result.push(result_json);
        lock_result.unlock();
        data->cv_result.notify_one();
    }
}

void show_result(FrameInfo *data) {
    size_t recv_count = 0;
    while (++recv_count != data->frame_count) {
        std::unique_lock<std::mutex> lock_result(data->mtx_result);
        bool available = data->cv_result.wait_for(
            lock_result, std::chrono::milliseconds(CV_TIMEOUT),
            [&data] { return !data->result.empty(); });
        if (!available) {
            std::cout << "Result data did not reach the queue and timed out"
                      << std::endl;
            break;
        }

        auto result_json = data->result.front();
        data->result.pop();
        lock_result.unlock();
        data->cv_result.notify_one();

        std::unique_lock<std::mutex> lock_in_(data->mtx_in_);
        available = data->cv_result.wait_for(
            lock_in_, std::chrono::milliseconds(CV_TIMEOUT),
            [&data] { return !data->image_in_.empty(); });
        if (!available) {
            std::cout
                << "Image for Result data did not reach the queue and timed out"
                << std::endl;
            break;
        }
        cv::Mat frame = data->image_in_.front();
        data->image_in_.pop();
        lock_in_.unlock();
        data->cv_in_.notify_one();
        std::vector<std::vector<int>> limb_seq = {
            {0, 1}, {1, 2}, {2, 3},  {3, 4},  {1, 5},   {5, 6},  {6, 7},
            {1, 8}, {8, 9}, {9, 10}, {1, 11}, {11, 12}, {12, 13}};
        auto result_poses = result_json.at("poses");
        for (const auto &outer_pair : result_poses.as_object()) {
            const auto &outer_key = outer_pair.key();
            const auto &outer_value = outer_pair.value();
            cv::Point2f pose_points[14];
            int i = 0;
            for (const auto &middle_pair : outer_value.as_object()) {
                const auto &middle_key = middle_pair.key();
                const auto &middle_value = middle_pair.value();
                double x = middle_value.at("x").as_double();
                double y = middle_value.at("y").as_double();
                int type = middle_value.at("type").as_int64();
                cv::Point2f point2f(x, y);
                pose_points[i++] = point2f;
                if (point2f != cv::Point2f(0, 0)) {
                    cv::circle(frame, point2f, 5, cv::Scalar(0, 255, 0), -1);
                }
            }
            for (size_t i = 0; i < limb_seq.size(); ++i) {
                cv::Point2f a = pose_points[limb_seq[i][0]];
                cv::Point2f b = pose_points[limb_seq[i][1]];
                if (a != cv::Point2f(0, 0) && b != cv::Point2f(0, 0)) {
                    cv::line(frame, a, b, cv::Scalar(255, 0, 0), 3, 4);
                }
            }
        }
        cv::imshow("result", frame);
        cv::waitKey(1);
    }
    cv::destroyAllWindows();
}

void read_image(FrameInfo *data) {
    while (true) {
        cv::Mat frame;
        data->cap >> frame;
        if (frame.empty()) {
            data->cap.release();
            break;
        }
        if (frame.cols != 368 || frame.rows != 368) {
            cv::resize(frame, frame, cv::Size(368, 368));
        }
        std::unique_lock<std::mutex> lock_in(data->mtx_in);
        data->image_in.push(frame);
        lock_in.unlock();
        data->cv_in.notify_one();

        std::unique_lock<std::mutex> lock_in_(data->mtx_in_);
        data->image_in_.push(frame);
        lock_in_.unlock();
        data->cv_in_.notify_one();
    }
}

int main(int argc, char *argv[]) {
    char *server_ip = argv[1];
    int server_port = std::stoi(argv[2]);
    std::string video_file = argv[3];

    boost::asio::io_service io_service;
    ip::tcp::resolver resolver(io_service);
    ip::tcp::resolver::query query(server_ip, std::to_string(server_port));
    ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    ip::tcp::socket socket(io_service);
    boost::asio::connect(socket, endpoint_iterator);
    socket.set_option(ip::tcp::no_delay(true));

    FrameInfo *data = new FrameInfo(cv::Mat(), std::move(socket), video_file);

    std::thread read_image_thread(read_image, data);
    std::thread send_frame_thread(send_frame, data);
    std::thread recv_result_thread(recv_result, data);
    std::thread show_result_thread(show_result, data);

    read_image_thread.join();
    std::cout << "Joined read_image_thread" << std::endl;
    send_frame_thread.join();
    std::cout << "Joined send_frame_thread" << std::endl;
    recv_result_thread.join();
    std::cout << "Joined recv_result_thread" << std::endl;
    show_result_thread.join();
    std::cout << "Joined show_result_thread" << std::endl;
    return 0;
}

