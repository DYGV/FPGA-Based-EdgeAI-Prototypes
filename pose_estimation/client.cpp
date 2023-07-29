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
        boost::json::value result = boost::json::parse(result_data);
        std::cout << result << std::endl;

        std::vector<std::vector<int>> limb_seq = {
            {0, 1}, {1, 2}, {2, 3},  {3, 4},  {1, 5},   {5, 6},  {6, 7},
            {1, 8}, {8, 9}, {9, 10}, {1, 11}, {11, 12}, {12, 13}};
        auto result_poses = result.at("poses");
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
                cv::circle(frame, point2f, 5, cv::Scalar(0, 255, 0), -1);
            }
            for (size_t i = 0; i < limb_seq.size(); ++i) {
                cv::Point2f a = pose_points[limb_seq[i][0]];
                cv::Point2f b = pose_points[limb_seq[i][1]];
                cv::line(frame, a, b, cv::Scalar(255, 0, 0), 3, 4);
            }
        }
        cv::imshow("result", frame);
        cv::waitKey(1);
    }
    socket.close();
    return 0;
}
