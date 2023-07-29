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

#include <iostream>
#include <fstream>
#include <string>
#include <boost/json.hpp>
#include <boost/json/src.hpp>
#include <opencv2/opencv.hpp>
#include <vitis/ai/openpose.hpp>

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

int main(int argc, char *argv[]) {
    // ./pose_estimation_simple ./foo.xmodel ./bar.jpg ./foobar.json
    if(argc < 4) {
        return -1;
    }
    std::string model_ = argv[1];
    std::unique_ptr<vitis::ai::OpenPose> model = vitis::ai::OpenPose::create(model_);

    std::string input_image_path = argv[2];
    std::string output_json_path = argv[3];
    cv::Mat frame = cv::imread(input_image_path);
    vitis::ai::OpenPoseResult result = model->run(frame);

    std::ofstream output_file_stream;
    output_file_stream.open(output_json_path, std::ios::out);
    output_file_stream << result_to_json_string(result);
    output_file_stream.close();

    return 0;
}
