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

#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <boost/json/src.hpp>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <queue>
#include <thread>
#include <vitis/ai/openpose.hpp>

#define DEFAULT_PORT 54321

#define WIDTH 368
#define HEIGHT 368
#define CHANNEL 3
#define FRAME_SIZE WIDTH *HEIGHT *CHANNEL

struct FrameInfo {
    FrameInfo(cv::Mat img, boost::asio::ip::tcp::socket sock)
        : image_in(std::queue<cv::Mat>()),
          result(std::queue<vitis::ai::OpenPoseResult>()),
          socket(std::move(sock)), already_stopped(false) {}

    std::queue<cv::Mat> image_in;
    std::queue<vitis::ai::OpenPoseResult> result;
    boost::asio::ip::tcp::socket socket;
    std::mutex mtx_in;
    std::mutex mtx_result;
    std::condition_variable cv_in;
    std::condition_variable cv_result;
    bool already_stopped;
};

std::unique_ptr<vitis::ai::OpenPose> model;
std::mutex mtx_dpu;

void pose_estimate(std::shared_ptr<FrameInfo> data) {
    while (true) {
        std::unique_lock<std::mutex> lock_in(data->mtx_in);
        data->cv_in.wait_for(lock_in, std::chrono::milliseconds(5000),
                             [&data] { return !data->image_in.empty(); });
        if (data->image_in.empty()) {
            lock_in.unlock();
            data->cv_in.notify_one();
            if (data->already_stopped) {
                return;
            } else {
                continue;
            }
        }

        cv::Mat image = data->image_in.front();
        data->image_in.pop();
        lock_in.unlock();
        data->cv_in.notify_one();

        std::unique_lock<std::mutex> lock_dpu(mtx_dpu);
        vitis::ai::OpenPoseResult result = model->run(image);
        lock_dpu.unlock();

        std::unique_lock<std::mutex> lock_result(data->mtx_result);
        data->result.push(result);
        lock_result.unlock();
        data->cv_result.notify_one();
    }
}

std::string result_to_json_string(const vitis::ai::OpenPoseResult &result) {
    boost::json::object result_json;
    boost::json::object poses;
    int num = 0;
    result_json["height"] = result.height;
    result_json["width"] = result.width;
    result_json["num"] = result.poses.size();
    for (const auto &pose : result.poses) {
        boost::json::object pose_point_json;
        int pose_num = 0;
        for (const auto &point : pose) {
            boost::json::value point_json = {{"type", point.type},
                                             {"x", point.point.x},
                                             {"y", point.point.y}};
            pose_point_json[std::to_string(pose_num++)] = std::move(point_json);
        }
        poses[std::to_string(++num)] = std::move(pose_point_json);
    }
    result_json["poses"] = std::move(poses);
    std::string serialized_data = boost::json::serialize(result_json);
    return serialized_data;
}

void tcp_send(std::shared_ptr<FrameInfo> data) {
    while (true) {
        std::unique_lock<std::mutex> lock_result(data->mtx_result);
        data->cv_result.wait_for(lock_result, std::chrono::milliseconds(5000),
                                 [&data] { return !data->result.empty(); });
        if (data->result.empty()) {
            lock_result.unlock();
            data->cv_result.notify_one();
            if (data->already_stopped) {
                return;
            } else {
                continue;
            }
        }

        vitis::ai::OpenPoseResult result = data->result.front();
        data->result.pop();

        lock_result.unlock();
        data->cv_result.notify_one();

        std::string serialized_data = result_to_json_string(result);
        std::size_t data_size = serialized_data.size();

        async_write(data->socket,
                    boost::asio::buffer(&data_size, sizeof(std::size_t)),
                    [&data](const boost::system::error_code &ec, size_t) {
                        if (ec) {
                            std::cerr << "Error sending image: " << ec.message()
                                      << std::endl;
                            data->already_stopped = true;
                        }
                    });

        async_write(data->socket,
                    boost::asio::buffer(serialized_data, data_size),
                    [&data](const boost::system::error_code &ec, size_t) {
                        if (ec) {
                            std::cerr << "Error sending image: " << ec.message()
                                      << std::endl;
                            data->already_stopped = true;
                        }
                    });
    }
}

void tcp_recv(std::shared_ptr<FrameInfo> data) {
    while (true) {
        boost::system::error_code ec;
        uchar buf[FRAME_SIZE];

        boost::system::error_code error;
        boost::asio::read(data->socket, boost::asio::buffer(buf, FRAME_SIZE),
                          error);
        if (error) {
            std::cerr << "Error while receiving data: " << error.message()
                      << std::endl;
            data->already_stopped = true;
            return;
        }
        cv::Mat image = cv::Mat(HEIGHT, WIDTH, CV_8UC3, buf);
        std::unique_lock<std::mutex> lock_in(data->mtx_in);
        data->image_in.push(image);
        lock_in.unlock();
        data->cv_in.notify_one();
    }
}

void client_handler(boost::asio::ip::tcp::socket socket) {
    auto client_addr = socket.remote_endpoint();
    auto client_data =
        std::make_shared<FrameInfo>(cv::Mat(), std::move(socket));
    std::thread tcp_recv_thread(tcp_recv, client_data);
    std::thread pose_estimate_thread(pose_estimate, client_data);
    std::thread tcp_send_thread(tcp_send, client_data);

    tcp_recv_thread.join();
    pose_estimate_thread.join();
    tcp_send_thread.join();
    std::cout << "Connection to " << client_addr << " is now fully closed"
              << std::endl;
}

int main(int argc, char *argv[]) {
    std::string model_ = argv[1];
    int port = DEFAULT_PORT;
    if (argc > 2) {
        port = std::stoi(argv[2]);
    }

    model = vitis::ai::OpenPose::create(model_);

    boost::asio::io_service service;
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
    boost::asio::ip::tcp::acceptor acceptor(service, endpoint);
    std::cout << "Launched pose estimation server" << std::endl;

    while (true) {
        boost::asio::ip::tcp::socket sock(service);
        acceptor.accept(sock);
        std::cout << "New client: " << sock.remote_endpoint() << std::endl;
        std::thread client_handler_thread(client_handler, std::move(sock));
        client_handler_thread.detach();
    }
    return 0;
}
