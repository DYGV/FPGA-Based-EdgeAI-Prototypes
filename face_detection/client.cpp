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
#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>

using namespace boost::asio;
using namespace std;

int main(int argc, char *argv[]) {
    if (argc < 4) {
        return 1;
    }

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

    cv::VideoCapture cap(video_file);
    if (!cap.isOpened()) {
        return 1;
    }

    while (true) {
        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) {
            break;
        }
        std::size_t frame_size = frame.total() * frame.elemSize();
        boost::asio::write(socket, boost::asio::buffer(frame.data, frame_size));
        size_t result_size;
        boost::asio::read(
            socket, boost::asio::buffer(&result_size, sizeof(std::size_t)));
        string result_data(result_size, '\0');
        boost::asio::read(socket,
                          boost::asio::buffer(&result_data[0], result_size));
        boost::json::value result_json = boost::json::parse(result_data);
        std::cout << result_json << std::endl;
        int num = result_json.as_object()["num"].as_int64();
        std::cout << "Number of faces detected: " << num << std::endl;
        for (int i = 1; i <= num; ++i) {
            auto &face = result_json.as_object()[std::to_string(i)].as_object();
            int x = face["x"].as_int64();
            int y = face["y"].as_int64();
            int size_col = face["size_col"].as_int64();
            int size_row = face["size_row"].as_int64();
            std::cout << "Face " << i << ": x=" << x << " y=" << y
                 << " width=" << size_col << " height=" << size_row << std::endl;
            cv::rectangle(
                frame, cv::Rect{cv::Point(x, y), cv::Size{size_col, size_row}},
                cv::Scalar(255, 0, 0), 3, 3);
        }
        cv::imshow("result", frame);
        cv::waitKey(1);
    }

    socket.close();
    return 0;
}
