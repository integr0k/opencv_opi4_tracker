#include <opencv2/core/version.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>


int main() {
    std::string url = "rtsp://192.168.1.82:8080/h264_opus.sdp";
    std::vector<int> params = {
        cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 5000,
        cv::CAP_PROP_READ_TIMEOUT_MSEC, 5000 
    };

    cv::dnn::Net net = cv::dnn::readNetFromONNX("yolo11n.onnx");
    cv::VideoCapture cam(url, cv::CAP_FFMPEG, params);

    if (!cam.isOpened()) {
        std::cout << "неудалось подключиться" << std::endl;
        return -1;
    }
    std::cout << "удалось подключиться" << std::endl;

    cv::namedWindow("window_cam", cv::WINDOW_NORMAL);

    cv::Mat frame;
    while (true) {
        cam >> frame;
        if (frame.empty()) {
            std::cout << "не удалось получить кадр" << std::endl;
            break;
        }

        cv::Mat blob;
        cv::dnn::blobFromImage(frame, blob, 1.0 / 255.0, cv::Size(640, 640), cv::Scalar(), true, false);
        net.setInput(blob);
        std::vector<cv::Mat> outputs;
        net.forward(outputs, net.getUnconnectedOutLayersNames());
        cv::Mat output = outputs[0];
        output = output.reshape(1, output.size[1]); 
        cv::transpose(output, output);

        std::vector<cv::Rect> boxes;
        std::vector<float> confidences;
        std::vector<int> classIds;

        float score_threshold = 0.25;

        for (int i = 0; i < output.rows; ++i) {
            cv::Mat classes_scores = output.row(i).colRange(4, output.cols);
            cv::Point classIdPoint;
            double maxClassScore;
            cv::minMaxLoc(classes_scores, 0, &maxClassScore, 0, &classIdPoint);

            if (maxClassScore > score_threshold) {
                float cx = output.at<float>(i, 0);
                float cy = output.at<float>(i, 1);
                float w  = output.at<float>(i, 2);
                float h  = output.at<float>(i, 3);
                int left   = static_cast<int>((cx - w / 2) * frame.cols / 640.0);
                int top    = static_cast<int>((cy - h / 2) * frame.rows / 640.0);
                int width  = static_cast<int>(w * frame.cols / 640.0);
                int height = static_cast<int>(h * frame.rows / 640.0);

                boxes.push_back(cv::Rect(left, top, width, height));
                confidences.push_back(static_cast<float>(maxClassScore));
                classIds.push_back(classIdPoint.x);
            }
        }

        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, score_threshold, 0.45, indices);

        for (int idx : indices) {
            cv::Rect box = boxes[idx];
            cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);
            std::string label = "ID: " + std::to_string(classIds[idx]);
            cv::putText(frame, label, cv::Point(box.x, box.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        }
        cv::imshow("window_cam", frame);
        cv::waitKey(1);

    }

    cam.release();
    cv::destroyAllWindows();
    return 0;
}
