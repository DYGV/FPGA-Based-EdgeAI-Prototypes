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

#define CV_TIMEOUT 2000
#define SLEEP_SEND_FRAME 10

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
    while (++recv_count <= data->frame_count) {
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
    while (++recv_count <= data->frame_count) {
        size_t result_size;
        boost::asio::read(data->socket, boost::asio::buffer(
                                            &result_size, sizeof(std::size_t)));
        std::string result_data(result_size, '\0');
        boost::asio::read(
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
    while (++recv_count <= data->frame_count) {
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

        int num = result_json.as_object()["num"].as_int64();
        for (int i = 1; i <= num; ++i) {
            auto &face = result_json.as_object()[std::to_string(i)].as_object();
            int x = face["x"].as_int64();
            int y = face["y"].as_int64();
            int size_col = face["size_col"].as_int64();
            int size_row = face["size_row"].as_int64();
            std::cout << "Face " << i << ": x=" << x << " y=" << y
                      << " width=" << size_col << " height=" << size_row
                      << std::endl;
            cv::rectangle(
                frame, cv::Rect{cv::Point(x, y), cv::Size{size_col, size_row}},
                cv::Scalar(255, 0, 0), 3, 3);
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
        if (frame.cols != 640 || frame.rows != 360) {
            cv::resize(frame, frame, cv::Size(640, 360));
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
