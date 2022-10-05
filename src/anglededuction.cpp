#include <cameraGeneric.hpp>

int threshold_low = 20;
int threshold_high = 255;
std::string pre_img_path = "/home/vittorio/coilsCL/imgs/Pre_Insertion_IMG.png";
std::string post_img_path = "/home/vittorio/coilsCL/imgs/Post_Insertion_IMG.png";

bool xWiseSort(Point lhs, Point rhs){
    return (lhs.x < rhs.x);
}

std::vector<Point> findJoints(Mat pre_img, Mat post_img);

int main(int argc, char* argv[]){
    Mat pre_img = imread(pre_img_path, IMREAD_COLOR);
    Mat post_img = imread(post_img_path, IMREAD_COLOR);

    if(pre_img.empty()) {
        std::cout << "Could not find image at: " << pre_img_path << "\n"; return -1;
    }
    if(post_img.empty()) {
        std::cout << "Could not find image at: " << post_img_path << "\n"; return -1;
    }

    int rcols, rrows;
    rcols = pre_img.rows * 3 / 8;
    rrows = pre_img.cols * 3 / 8;
    resize(pre_img, pre_img, Size(rrows, rcols), INTER_LINEAR);
    resize(post_img, post_img, Size(rrows, rcols), INTER_LINEAR);

    std::vector<Point> Joints;
    Joints = findJoints(pre_img, post_img);

    
    for(auto i: Joints){
        circle(post_img, i, 4, Scalar(255,0,0), FILLED);        
    }
    
    std::vector<double> angles;
    for(int i = 1; i < Joints.size(); i++){
        std::cout << Joints[i] << " ";
        double ratio = ( Joints[i].x - Joints[i-1].x ) / ( Joints[i].y - Joints[i-1].y );
        double theta = atan(ratio);
        if(theta < 0) theta = M_PI_2 - abs(theta);
        std::cout << "angle " << theta * 180 / M_PI_2 << "\n";

        Rect recta(Joints[i], Joints[i-1]);
        rectangle(post_img, recta, Scalar(0,0,255), 1);
        line(post_img, Joints[i], Joints[i-1], Scalar(255,0,0), 1);
    }

    imshow("Post", post_img);
    char c = (char)waitKey(0);
    destroyAllWindows();
    imwrite("/home/vittorio/coilsCL/imgs/Processed_IMG.png", post_img);
    return 0;


}

Mat IntroducerMask(Mat src){
    Mat src_GRAY, element;
    //create a greyscale copy of the image
    // flip(src, src, 1);

    cvtColor(src, src_GRAY, COLOR_BGR2GRAY);
    
    //apply blur and threshold so that only the tentacle is visible
    blur(src_GRAY, src_GRAY, Size(5,5));
    threshold(src_GRAY, src_GRAY, threshold_low, threshold_high, THRESH_BINARY_INV); 
    
    element = getStructuringElement(MORPH_DILATE, Size(7,7) );
    dilate(src_GRAY, src_GRAY, element);
    
 
    bitwise_not(src_GRAY, src_GRAY);

    return src_GRAY;

}

std::vector<Point> findJoints(Mat pre_img, Mat post_img){
    Mat post_img_grey, post_img_th, post_img_masked;

    Mat intr_mask = Mat::zeros(pre_img.size(), CV_8UC1);
    intr_mask = IntroducerMask(pre_img);



    cvtColor(post_img, post_img_grey, COLOR_BGR2GRAY);
    threshold(post_img_grey, post_img_th, 50, 250, THRESH_BINARY_INV);
    post_img_th.copyTo(post_img_masked, intr_mask);

    Mat contours_bin;

    std::vector<std::vector<Point> > contours;
    std::vector<Vec4i> hierarchy;
    //find contours, ignore hierarchy
    findContours(post_img_masked, contours, hierarchy, RETR_LIST, CHAIN_APPROX_SIMPLE);
    contours_bin = Mat::zeros(post_img_masked.size(), CV_8UC1);

    //draw contours and fill the open area
    drawContours(contours_bin, contours, -1, Scalar(255,255,255), cv::FILLED, LINE_8, hierarchy);
    //empty matrix. Set up to 8-bit 1 channel data. Very important to set up properly.
    Mat skeleton = Mat::zeros(post_img_masked.rows, post_img_masked.rows, CV_8U);
    
    //take the filled contour and thin it using Zhang Suen method. Only works with 8-bit 1 channel data. 
    ximgproc::thinning(contours_bin, skeleton, 0);
    
    contours.clear();
    hierarchy.clear();
    findContours(skeleton, contours, hierarchy, RETR_LIST, CHAIN_APPROX_SIMPLE);

    std::vector<Point> cntLine;
    findNonZero(skeleton, cntLine);
    std::sort(cntLine.begin(), cntLine.end(), xWiseSort);

    

    std::vector<Point> Joints;
    int jointCount = (int) cntLine.size() / 150;
    if(jointCount){
        for(int i = 0; i < jointCount; i++){
            Joints.push_back(cntLine[150*(i)]);
        }
    }

    Point endpoint;

    //iterate over all points
    for(auto i: cntLine){
        int neighbor_counter = 0;
        for(int j = -1; j < 2; j++){
            for(int k = -1; k < 2; k++){
                if(j == 0 && k == 0) continue;
                //count neighbors immediately adjecent
                if( (int) skeleton.at<uchar>(i.y+k, i.x+j) > 0  ) neighbor_counter++;
            }
        }
        //last point to measure 1 neighbor exactly must be the end of the line
        if(neighbor_counter == 1) endpoint = i;
    }
    Joints.push_back(endpoint);

    return Joints;
}