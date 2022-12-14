#include <cameraGeneric.hpp>


int g_slider = 60; //slider pos value
int g_slider_max = 200; //slider max value
const String window_capture_name = "Video Capture";

void on_Link_Length_trackbar(int, void*)
{
    // printf("%d\n", g_slider);
}


double meanError(std::vector<double> &desired, std::vector<double> &observed);

std::vector<Point> findJoints(Mat post_img_masked, std::vector<std::vector<Point>> &contours);

template<typename T>
double avgVect(std::vector<T> inputVec){
    double avg, sum = 0;

    for(auto i : inputVec){
        sum += i;
    }
    avg = sum / inputVec.size();
    return avg;
}


std::vector<double> computeAngles(std::vector<Point> Joints){
    std::vector<double> angles;
    std::vector<Point> vects;

    Joints.insert(Joints.begin(), Point(Joints[0].x, 0));
    for(int i = 1; i < Joints.size(); i++){
        vects.push_back(Point{Joints[i].x - Joints[i-1].x, Joints[i].y - Joints[i-1].y}  );
    }
    for(int i = 0; i < vects.size()-1; i++){
        double dproduct = vects[i].dot(vects[i+1]);
        double nproduct = norm(vects[i]) * norm(vects[i+1]);
        double th = acos(dproduct/nproduct);
        angles.push_back(th * 180 / M_PI);
    }
    std::reverse(angles.begin(), angles.end());
    return angles;

}


std::vector<Point> computeIdealPoints(Point p0, std::vector<double> desiredAngles){
    std::vector<Point> ideal;

    ideal.push_back(p0);
    for(int i = 1; i < desiredAngles.size(); i++){
        double angle = 0;
        for( int k = 0; k < i; k++) angle += desiredAngles[k];
        int xdiff = (g_slider) * sin(angle * M_PI / 180);
        int ydiff = (g_slider) * cos(angle * M_PI / 180);
        Point pn = Point{ (int) (ideal[i-1].x + xdiff), (int) ( ideal[i-1].y + ydiff )}; 
        ideal.push_back(pn);
    }

    return ideal;
}


inline bool file_exists (const std::string& name) {
    struct stat buffer;   
    return (stat (name.c_str(), &buffer) == 0); 
}

int main(int argc, char* argv[]){


    namedWindow(window_capture_name);
    createTrackbar("Low Thresh", window_capture_name, &g_slider, g_slider_max, on_Link_Length_trackbar);

    Mat pre_img, post_img, intr_mask;
    

    /*pylon video input here
    -----------------------------------------------------------*/
    Pylon::PylonInitialize();
    Pylon::CImageFormatConverter formatConverter;
    formatConverter.OutputPixelFormat = Pylon::PixelType_BGR8packed;
    Pylon::CPylonImage pylonImage;
    Pylon::CInstantCamera camera(Pylon::CTlFactory::GetInstance().CreateFirstDevice() );
    camera.Open();
    Pylon::CIntegerParameter width     ( camera.GetNodeMap(), "Width");
    Pylon::CIntegerParameter height    ( camera.GetNodeMap(), "Height");
    Pylon::CEnumParameter pixelFormat  ( camera.GetNodeMap(), "PixelFormat");
    Pylon::CFloatParameter(camera.GetNodeMap(), "ExposureTime").SetValue(20000.0);
    Size frameSize= Size((int)width.GetValue(), (int)height.GetValue());
    int codec = VideoWriter::fourcc('M', 'J', 'P', 'G');
    width.TrySetValue(PYLON_WIDTH, Pylon::IntegerValueCorrection_Nearest);
    height.TrySetValue(PYLON_HEIGHT, Pylon::IntegerValueCorrection_Nearest);
    Pylon::CPixelTypeMapper pixelTypeMapper( &pixelFormat);
    Pylon::EPixelType pixelType = pixelTypeMapper.GetPylonPixelTypeFromNodeValue(pixelFormat.GetIntValue());
    camera.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly);
    Pylon::CGrabResultPtr ptrGrabResult;
    camera.RetrieveResult(5000, ptrGrabResult, Pylon::TimeoutHandling_ThrowException);
    const uint8_t* preImageBuffer = (uint8_t*) ptrGrabResult->GetBuffer();
    formatConverter.Convert(pylonImage, ptrGrabResult);
    pre_img = cv::Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3, (uint8_t *) pylonImage.GetBuffer());
    /*-----------------------------------------------------------
    pylon video input here*/


    int rcols, rrows;
    rcols = pre_img.cols * 3 / 8;
    rrows = pre_img.rows * 3 / 8;
    
    std::string outputPath = "AD_LIVE.avi";

    while(file_exists(outputPath)){
        outputPath += "_1";
    }


    VideoWriter video_out(outputPath, VideoWriter::fourcc('M', 'J', 'P', 'G'), 10, 
                Size(rrows, rcols));
    
    resize(pre_img, pre_img, Size(rcols, rrows), INTER_LINEAR);

    // camera.RetrieveResult(5000, ptrGrabResult, Pylon::TimeoutHandling_ThrowException);
    // const uint8_t* pImageBuffer = (uint8_t*) ptrGrabResult->GetBuffer();
    // formatConverter.Convert(pylonImage, ptrGrabResult);
    // pre_img = cv::Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3, (uint8_t *) pylonImage.GetBuffer());
    intr_mask = IntroducerMask(pre_img);
    int jointsCached = 0;
    int k = 0;
    // std::cout << " Made it to while loop\n";
    Point p0 = Point{-2000,2000};
    while(camera.IsGrabbing()){
        camera.RetrieveResult(5000, ptrGrabResult, Pylon::TimeoutHandling_ThrowException);
        const uint8_t* pImageBuffer = (uint8_t*) ptrGrabResult->GetBuffer();
        formatConverter.Convert(pylonImage, ptrGrabResult);
        post_img = cv::Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3, (uint8_t *) pylonImage.GetBuffer());

        if(post_img.empty())
        {
            break;
        }
        resize(post_img, post_img, Size(rrows, rcols), INTER_LINEAR);
        Mat post_img_grey, post_img_th, post_img_masked;

        cvtColor(post_img, post_img_grey, COLOR_BGR2GRAY);
        blur(post_img_grey, post_img_grey, Size(5,5));
        threshold(post_img_grey, post_img_th, threshold_low, threshold_high, THRESH_BINARY_INV);
        post_img_th.copyTo(post_img_masked);
        // post_img_masked = post_img_th;

        std::vector<Point> Joints;
        std::vector<std::vector<Point>> contours;
        Joints = findJoints(post_img_masked, contours);
        
        std::vector<double> angles; 
        std::vector<double> desiredAngles = {10,15,15,20,20};
        std::vector<Point> idealPoints;
        //if(p0 == Point{-2000,2000}) 
        p0 = Joints[0];

        idealPoints = computeIdealPoints(p0, desiredAngles);
        angles = computeAngles(Joints);
        for(int i = 0; i < idealPoints.size()-1; i++){
            line(post_img, idealPoints[i], idealPoints[i+1], Scalar(0,0,255));
            circle(post_img, idealPoints[i], 2, Scalar(255,0,0));
        }
        
        
        
        
        
        int JointsObserved = Joints.size();
        drawContours(post_img, contours, -1, Scalar(255,0,0), FILLED, LINE_8);

        for(auto i: Joints){
            circle(post_img, i, 4, Scalar(255,0,0), FILLED);        
        }

        

        

        jointsCached = JointsObserved;
        std::vector<double> dAngleSlice = std::vector<double>(desiredAngles.end()-angles.size(), desiredAngles.end());         
        double error = meanError(dAngleSlice, angles);
        // }
        // for(int i = 1; i < JointsObserved; i++){
        //     Rect recta(Joints[i], Joints[i-1]);
        //     rectangle(post_img, recta, Scalar(0,0,255), 1);
        //     line(post_img, Joints[i], Joints[i-1], Scalar(255,0,0), 1);
        // }
        
        imshow(window_capture_name, post_img);
        video_out.write(post_img);
        char c= (char)waitKey(1e2);
        if(c==27) break;
        
        std::cout << "\rObserved Angles ";
        for(auto j: angles){
            std::cout << j << " ";
        }
        std::cout << "\nDesired angles ";
        for(auto j: desiredAngles){
            std::cout << j << " ";
        }
        std::cout << "\n";
        std::cout << "Therefore error " << error << "\n";
        std::cout << "\n------------------------------------\n";

        // std::cout << "\r" 
        // << std::setw(9) << std::setfill('0') << "\rError " << std::setprecision(4) << error << 
        // std::setw(8) << " Length " << g_slider << 
        // std::setw(8) << " Centre-line size:" << contours.size() <<
        // std::setw(8) << " joint no: " << Joints.size()  << std::flush;
        // std::cout << "----------------------------------------------------------------\n";
    }


    // std::cout <<"Average angles\n";
    // double avg1, avg2, avg3, avg4, avg5;
    // avg1 = avgVect(th1);
    // avg2 = avgVect(th2);
    // avg3 = avgVect(th3);
    // avg4 = avgVect(th4);
    // avg5 = avgVect(th5);
    // std::cout << avg1 << " " << avg2 << " " << avg3 << " " << avg4 << " " << avg5 << "\n";

    
    Pylon::PylonTerminate();
    destroyAllWindows();
    video_out.release();
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
    
    element = getStructuringElement(MORPH_DILATE, Size(3,3) );
    dilate(src_GRAY, src_GRAY, element);
    
 
    bitwise_not(src_GRAY, src_GRAY);

    return src_GRAY;

}

std::vector<Point> findJoints(Mat post_img_masked, std::vector<std::vector<Point>> &contours){
    

    Mat contours_bin;

    // std::vector<std::vector<Point> > contours;
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
    std::sort(cntLine.begin(), cntLine.end(), yWiseSort);
    // std::reverse(cntLine.begin(), cntLine.end());
    
    std::vector<Point> Joints;
    int jointCount = (int) cntLine.size() / g_slider;
    
    
    if(jointCount){
        for(int i = 0; i < jointCount; i++){
            Joints.push_back(cntLine[g_slider*(i)]);
        }
    }
    std::reverse(Joints.begin(), Joints.end());
    return Joints;
}

double meanError(std::vector<double> &desired, std::vector<double> &observed){
    double error, sum = 0;
    for(size_t i = 0; i < desired.size(); i++){
        sum += desired[i] - observed[i];
    }
    error = sum / desired.size();

    return error;
}