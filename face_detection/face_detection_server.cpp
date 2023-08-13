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
#include <vitis/ai/facedetect.hpp>

#define DEFAULT_PORT 54321

#define WIDTH 640
#define HEIGHT 360
#define CHANNEL 3
#define FRAME_SIZE WIDTH *HEIGHT *CHANNEL

using namespace boost::asio;

struct FrameInfo {
    FrameInfo(cv::Mat img, ip::tcp::socket sock)
        : image_in(std::queue<cv::Mat>()),
          result(std::queue<vitis::ai::FaceDetectResult>()),
          socket(std::move(sock)) {}

    std::queue<cv::Mat> image_in;
    std::queue<vitis::ai::FaceDetectResult> result;
    ip::tcp::socket socket;
    std::mutex mtx_in;
    std::mutex mtx_result;
    std::condition_variable cv_in;
    std::condition_variable cv_result;
};

std::unique_ptr<vitis::ai::FaceDetect> model;
std::mutex mtx_dpu;

void face_detect(FrameInfo *data) {
    while (true) {
        std::unique_lock<std::mutex> lock_in(data->mtx_in);
        data->cv_in.wait(lock_in, [&data] { return !data->image_in.empty(); });

        cv::Mat image = data->image_in.front();
        data->image_in.pop();
        lock_in.unlock();


        std::unique_lock<std::mutex> lock_dpu(mtx_dpu);
        vitis::ai::FaceDetectResult result = model->run(image);
        lock_dpu.unlock();

        std::unique_lock<std::mutex> lock_result(data->mtx_result);
        data->result.push(result);
        lock_result.unlock();
        data->cv_result.notify_one();
    }
}

std::string result_to_json_string(const vitis::ai::FaceDetectResult &result) {
    boost::json::object result_json;
    result_json["num"] = result.rects.size();
    result_json["height"] = result.height;
    result_json["width"] = result.width;
    int num = 0;
    for (const auto &r : result.rects) {
        boost::json::object result_json_pos;
        result_json_pos["x"] = int(r.x * result.width);
        result_json_pos["y"] = int(r.y * result.height);
        result_json_pos["size_col"] = int(r.width * result.width);
        result_json_pos["size_row"] = int(r.height * result.height);
        result_json[std::to_string(++num)] = std::move(result_json_pos);
    }
    std::string serialized_data = boost::json::serialize(result_json);
    return serialized_data;
}

void tcp_send(FrameInfo *data) {
    io_service service;

    while (true) {
        std::unique_lock<std::mutex> lock_result(data->mtx_result);
        data->cv_result.wait(lock_result,
                             [&data] { return !data->result.empty(); });

        vitis::ai::FaceDetectResult result = data->result.front();
        data->result.pop();

        lock_result.unlock();
        data->cv_result.notify_one();

        std::string serialized_data = result_to_json_string(result);
        std::size_t data_size = serialized_data.size();

        try {
            async_write(data->socket, buffer(&data_size, sizeof(std::size_t)),
                        [](const boost::system::error_code &ec, size_t) {
                            if (ec) {
                                std::cerr
                                    << "Error sending image: " << ec.message()
                                    << std::endl;
                            }
                        });

            async_write(data->socket, buffer(serialized_data, data_size),
                        [](const boost::system::error_code &ec, size_t) {
                            if (ec) {
                                std::cerr
                                    << "Error sending image: " << ec.message()
                                    << std::endl;
                            }
                        });
        } catch (std::exception &e) {
            std::cerr << "Exception: " << e.what() << std::endl;
        }
    }
}

void tcp_recv(FrameInfo *data) {
    while (true) {
        boost::system::error_code ec;
        uchar buf[FRAME_SIZE];
        int bytes_received = 0;
        while (bytes_received < FRAME_SIZE) {
            boost::system::error_code error;
            try {
                int ret = data->socket.read_some(
                    boost::asio::buffer(buf + bytes_received,
                                        FRAME_SIZE - bytes_received),
                    error);
                if (error) {
                    std::cerr
                        << "Error while receiving data: " << error.message()
                        << std::endl;

                    return;
                }
                if (ret == 0) {
                    std::cout << "Client disconnected" << std::endl;
                    return;
                }

                bytes_received += ret;
            } catch (std::exception &e) {
                std::cerr << "Exception in handleClient: " << e.what()
                          << std::endl;
            }
        }
        cv::Mat image = cv::Mat(HEIGHT, WIDTH, CV_8UC3, buf);
        std::unique_lock<std::mutex> lock_in(data->mtx_in);
        data->image_in.push(image);
        lock_in.unlock();
        data->cv_in.notify_one();
    }
}

int main(int argc, char *argv[]) {
    std::string model_ = argv[1];
    int port = DEFAULT_PORT;
    if (argc > 2) {
        port = std::stoi(argv[2]);
    }
    model = vitis::ai::FaceDetect::create(model_);

    io_service service;
    ip::tcp::endpoint endpoint(ip::tcp::v4(), port);
    ip::tcp::acceptor acceptor(service, endpoint);
    std::cout << "Launched face detection server" << std::endl;

    while (true) {
        ip::tcp::socket sock(service);
        acceptor.accept(sock);
        std::cout << "New client: "
                  << sock.remote_endpoint().address().to_string() << std::endl;

        FrameInfo *data = new FrameInfo(cv::Mat(), std::move(sock));

        std::thread tcp_recv_thread(tcp_recv, data);
        std::thread face_detect_thread(face_detect, data);
        std::thread tcp_send_thread(tcp_send, data);

        tcp_recv_thread.detach();
        face_detect_thread.detach();
        tcp_send_thread.detach();
    }
    return 0;
}

