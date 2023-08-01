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

#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <queue>
#include <thread>
#include <vector>
#include <vitis/ai/facedetect.hpp>

struct FrameInfo {
    FrameInfo(cv::Mat img)
        : image_in(std::queue<cv::Mat>()),
          result(std::queue<vitis::ai::FaceDetectResult>()) {}

    std::queue<cv::Mat> image_in;
    std::queue<vitis::ai::FaceDetectResult> result;
    std::vector<std::string> file_names;
    std::mutex mtx_in;
    std::mutex mtx_result;
    std::condition_variable cv_in;
    std::condition_variable cv_result;
    bool stop;
};

std::unique_ptr<vitis::ai::FaceDetect> model;

void face_detect(FrameInfo *data) {
    while (true) {
        std::unique_lock<std::mutex> lock_in(data->mtx_in);
        data->cv_in.wait_for(lock_in, std::chrono::milliseconds(500), 
                    [&data] { return !data->image_in.empty(); });
        if(data->stop) {
            break;
        }

        cv::Mat image = data->image_in.front();
        data->image_in.pop();
        lock_in.unlock();

        vitis::ai::FaceDetectResult result = model->run(image);

        std::unique_lock<std::mutex> lock_result(data->mtx_result);
        data->result.push(result);
        lock_result.unlock();
        data->cv_result.notify_one();
    }
}

void show_result(FrameInfo *data) {
    unsigned long image_count = 0;
    while (true) {
        std::unique_lock<std::mutex> lock_result(data->mtx_result);
        data->cv_result.wait_for(lock_result, std::chrono::milliseconds(500),
                             [&data] { return !data->result.empty(); });
        if(data->stop) {
            break;
        }
        vitis::ai::FaceDetectResult result = data->result.front();
        data->result.pop();
        lock_result.unlock();
        data->cv_result.notify_one();
        std::cout << "frame " << ++image_count << ": ";
        for (const auto &r : result.rects) {
            int x = r.x * result.width;
            int y = r.y * result.height;
            int size_col = r.width * result.width;
            int size_row = r.height * result.height;
            std::cout << "x = " << x << ", y = " << y
                      << ", size_col = " << size_col
                      << ", size_row = " << size_row;
        }
        std::cout << std::endl;
    }
}

void read_image(FrameInfo *data) {
    while (!data->file_names.empty()) {
        std::string path = data->file_names.front();
        data->file_names.erase(data->file_names.begin());
        cv::Mat image = cv::imread(path);
        std::unique_lock<std::mutex> lock_in(data->mtx_in);
        data->image_in.push(image);
        lock_in.unlock();
        data->cv_in.notify_one();
    }
    data->stop = true;
}

int main(int argc, char *argv[]) {
    std::string model_ = argv[1];
    std::string images_directory = argv[2];
    std::set<std::filesystem::path> contents_of_dir;
    // ディレクトリからファイル(画像)をすべて取得する
    std::filesystem::directory_iterator iter(images_directory), end;
    for (const auto &file :
         std::filesystem::directory_iterator(images_directory)) {
        // setを使って名前順にする
        contents_of_dir.insert(file.path());
    }
    // setからvectorに変換する
    std::vector<std::string> file_names(contents_of_dir.begin(),
                                        contents_of_dir.end());

    model = vitis::ai::FaceDetect::create(model_);

    FrameInfo *data = new FrameInfo(cv::Mat());
    data->file_names = file_names;
    data->stop = false;

    std::thread read_image_thread(read_image, data);
    std::thread face_detect_thread(face_detect, data);
    std::thread show_result_thread(show_result, data);

    read_image_thread.join();
    face_detect_thread.join();
    show_result_thread.join();
    return 0;
}
