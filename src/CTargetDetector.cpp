#include "CTargetDetector.h"
double TargetDetector::ALPHA=0;
double TargetDetector::DELTA=0;



TargetDetector::TargetDetector(int n_x, int n_y, bool is_thermal, bool draw){
    this->n_x = n_x;    
    this->n_y = n_y;
    this->prev_success=false;
    this->is_thermal = is_thermal;
    
    this->color_threshold_min = 60;//60; 
    this->size_threshold = 400;

    //detector1
    this->fullfill_threshold1 = 0.3; // rgb12: 0.1
    this->fullfill_threshold2 = 0.01;
    this->eccentricity_threshold = 0.1;

    this->draw=draw; 
    this->use_weight=false; 
    this->do_iterative_search=false;
    if(is_thermal){
        this->color_threshold_max = 150; 
        this->color_threshold_step = 5;
    }
    else{
        if(do_iterative_search){
            this->color_threshold_max = 255; 
            this->color_threshold_step = 5;
        }
        else this->color_threshold_max = 125;
    }

    this->text_colors;
    text_colors.push_back(cv::Scalar(255,0,0));
    text_colors.push_back(cv::Scalar(0,255,0));
    text_colors.push_back(cv::Scalar(0,0,255));
    text_colors.push_back(cv::Scalar(255,255,0));
    text_colors.push_back(cv::Scalar(255,0,255));
    text_colors.push_back(cv::Scalar(0,255,255));
    text_colors.push_back(cv::Scalar(255,255,255));
}
pair<bool,vector<cv::Point2f>> TargetDetector::detect(cv::Mat img, string type){
    bool ret;
    vector<cv::Point2f> target;
    if (type == "square"){
        // 순서 : y 높은순 ->  x 높은순
        ret = cv::findChessboardCorners(img,cv::Size(n_x,n_y), target); //flag????
        if(draw&&ret){
            cv::Mat bgr_img;
            cv::cvtColor(img,bgr_img, cv::COLOR_GRAY2BGR);
            cv::drawChessboardCorners(bgr_img, cv::Size(n_x,n_y),target,ret);
            cv::imshow("hi",bgr_img);
            cv::waitKey(0);
        }
        reverse(target.begin(), target.end());
    }
    else if(type == "circle"){

        // if(do_iterative_search){
        //     color_threshold = color_threshold_max;
        //     while(color_threshold>color_threshold_min){
        //         ret=detect_circles(img, target);
        //         if(ret) break;
        //         else color_threshold -= color_threshold_step;
        //     }
        // }
        // else{
        //     color_threshold = color_threshold_max;
        //     ret=detect_circles(img, target);
        // }
        // if(this->draw){
        //     if(!ret) color_threshold=125;
        //     detect_circles(img, target, true);
        //     printf("save: 1, ignore: 0\n");
        //     char key = cv::waitKey(0);
        //     while(key != '0' && key!='1'){
        //         printf("wrong commend is detected\n");
        //         key = cv::waitKey(0);
        //     }
        //     cv::destroyAllWindows();
        //     if(key == '0'){
        //         ret = false;
        //     }
        // }

        if(this->draw){
            ret= detect_circles2(img, target, true);
            printf("save: 1, ignore: 0\n");
            char key = cv::waitKey(0);
            while(key != '0' && key!='1'){
                printf("wrong commend is detected\n");
                key = cv::waitKey(0);
            }
            cv::destroyAllWindows();
            if(key == '0'){
                ret = false;
            }
        }
        else{
            ret= detect_circles2(img, target);
        }
        printf("color_threshold: %d\n",color_threshold);

        if(ret) prev_success=true;
    }
    else{
        throw WrongTypeException();
    }


    return make_pair(ret, target);
}

void TargetDetector::dfs(cv::Mat img, vector<vector<bool>> &buffer, vector<array<int,3>> &area, int x, int y){
    int W = img.cols;
    int H = img.rows;
    if(area.size()>W*H/4) return;
    stack<cv::Point2i> stack;
    buffer[y][x]=false;
    stack.push(cv::Point2i(x,y));
    while(stack.size()!=0){
        cv::Point2i curr_pt = stack.top();
        stack.pop();
        int x{curr_pt.x}, y{curr_pt.y};
        if( check_pixel(img, x, y)){
            area.push_back(array<int,3>{x,y,img.data[y*W+x]});
            if(x+1<W && buffer[y][x+1]) { buffer[y][x+1]=false; stack.push(cv::Point2i(x+1,y));}
            if(x>0 && buffer[y][x-1]) {buffer[y][x-1]=false; stack.push(cv::Point2i(x-1,y));}
            if(y+1<H && buffer[y+1][x]) {buffer[y+1][x]=false; stack.push(cv::Point2i(x,y+1));}
            if(y>0 && buffer[y-1][x]) {buffer[y-1][x]=false; stack.push(cv::Point2i(x,y-1));}
        }

    }
}
bool TargetDetector::check_pixel(cv::Mat img, int x,int y){
    int W = img.cols;
    return img.data[y*W+x] < color_threshold;
}

bool TargetDetector::ellipse_test(const vector<array<int,3>> &area, cv::Point2f &pt){
    //Todo fullfill test
    
    int n = area.size();

    if(n<size_threshold) return false;

    int x,y, maxx{-1},maxy{-1},minx{10000},miny{10000};

    double mx{0},my{0},cx{0}, cy{0},xx{0},xy{0},yy{0},weight_sum{0};
    double weight;
    for(int i=0;i<n;i++){
        if(use_weight) weight = (color_threshold-area[i][2]);
        else weight=1.0;

        x = area[i][0];
        y = area[i][1];
        // int weight = 1;
        minx = x<minx ? x: minx;
        miny = y<miny ? y: miny;
        maxx = x>maxx ? x: maxx;
        maxy = y>maxy ? y: maxy;
        cx+= x*weight;
        cy+= y*weight;
        mx+=x;
        my+=y;
        xx += x*x*1;
        yy += y*y*1;
        xy += x*y*1;
        
        weight_sum += weight;
    }

    double ratio1 = abs(1- (maxx-minx)*(maxy-miny)/4*M_PI/n);

    if( ratio1>fullfill_threshold1) {
        // if(draw) printf("fial ratio1: %f\n",ratio1);
        return false;
    }
    cx = cx/weight_sum;
    cy = cy/weight_sum;
    mx = mx/n;
    my = my/n;
    xx = xx/n - pow(mx,2);
    xy = xy/n - mx*my;
    yy = yy/n - pow(my,2);

    double det = (xx+yy)*(xx+yy)-4*(xx*yy-xy*xy);
    if (det > 0) det = sqrt(det); else det = 0;
    double f0 = ((xx+yy)+det)/2;
    double f1 = ((xx+yy)-det)/2;
    double m0 = sqrt(f0);
    double m1 = sqrt(f1);

    double ratio2 = abs(1- m0*m1*4*M_PI/n);

    if(ratio2>fullfill_threshold2) {
        // if(draw) printf("fial ratio2: %f\n",ratio2);
        return false;
    }

    if(m1/m0<eccentricity_threshold) {
        // if(draw) printf("fial ratio3: %f\n",m1/m0);
        return false;
    }

    pt.x = cx;
    pt.y = cy;

    return true;
}

bool TargetDetector::compare(cv::Point2i a, cv::Point2i b){
    double c  = cos(ALPHA);
    double s  = sin(ALPHA);
    double x1mx2 = c*(a.x-b.x)+s*(a.y-b.y);
    double y1my2 = -s*(a.x-b.x)+c*(a.y-b.y);
    bool aless; 

    if(abs(y1my2)<DELTA){
        aless = x1mx2<0;
    }
    else{
        aless = y1my2<0;
    }
	return aless; // true이면 a가 b 앞에 감. 즉 a가 b보다 앞일 조건 쓰면 됌
}

bool TargetDetector::detect_circles2(cv::Mat img, vector<cv::Point2f>&target, bool debug){

    cv::Mat img_origin, img_blur, img_thresh, img_morph, img_contour, img_output;

    cvtColor(img, img_origin, cv::COLOR_GRAY2BGR);
    cv::fastNlMeansDenoisingColored(img_origin, img_origin, 10, 10, 7, 21);
    cvtColor(img_origin, img_origin, cv::COLOR_BGR2GRAY);

    // img_origin  = img.clone();
    img_output = img.clone();
    cvtColor(img_output, img_output, cv::COLOR_GRAY2BGR);

    // collect all blob candidates //
    std::vector<std::vector<cv::Point>> circle_contour_candidates;
    cv::GaussianBlur(img_origin, img_blur, cv::Size(5, 5), 0);
    for (int bs = 3; bs <= 19; bs += 2)
    {
        cv::adaptiveThreshold(img_blur, img_thresh, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY_INV, bs, 2);

        for (int s = 1; s <= 5; s += 2)
        {
            
            cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(s, s));
            cv::morphologyEx(img_thresh, img_morph, cv::MORPH_CLOSE, kernel);

            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(img_morph, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

            for (size_t i = 0; i < contours.size(); i++)
            {
                double area = cv::contourArea(contours[i]);
                double perimeter = cv::arcLength(contours[i], true);
                double circularity = abs(1 - (4 * M_PI * area) / (perimeter * perimeter));

                if (area > size_threshold && circularity < 0.5)
                {
                    circle_contour_candidates.push_back(contours[i]);
                }
            }
        }
    }

    // select valid blobs //
   std::vector<std::vector<cv::Point>> circle_contours;
    for (size_t i = 0; i < circle_contour_candidates.size(); i++)
    {
        cv::Moments m1 = cv::moments(circle_contour_candidates[i]);
        double m1_area = cv::contourArea(circle_contour_candidates[i]);

        if (circle_contours.size() == 0)
        {
            circle_contours.push_back(circle_contour_candidates[i]);
        }
        else
        {
            bool overlap = false;
            for (size_t j = 0; j < circle_contours.size(); j++)
            {
                cv::Moments m2 = cv::moments(circle_contours[j]);

                if (m1.m00 != 0 && m2.m00 != 0)
                {
                    cv::Point m1_c = cv::Point(int(m1.m10 / m1.m00), int(m1.m01 / m1.m00));
                    cv::Point m2_c = cv::Point(int(m2.m10 / m2.m00), int(m2.m01 / m2.m00));

                    float dist = sqrt(pow(m1_c.x - m2_c.x, 2) + pow(m1_c.y - m2_c.y, 2));
                    if (dist < 5.)
                    {
                        overlap = true;
                        cv::Mat mask_overlap = cv::Mat::zeros(img_origin.size(), CV_8UC1);
                        std::vector<std::vector<cv::Point>> cnt_overlap;
                        cnt_overlap.push_back(circle_contours[j]);
                        cnt_overlap.push_back(circle_contour_candidates[i]);
                        cv::drawContours(mask_overlap, cnt_overlap, 0, cv::Scalar(255, 255, 255), cv::FILLED);
                        cv::drawContours(mask_overlap, cnt_overlap, 1, cv::Scalar(255, 255, 255), cv::FILLED);

                        std::vector<std::vector<cv::Point>> cnt;
                        cv::findContours(mask_overlap, cnt, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

                        circle_contours[j] = cnt[0];

                        // if (m1.m00 > m2.m00)
                        // {
                        //     circle_contours[j] = circle_contour_candidates[i];
                        // }


                        break;
                    }  
                }
            }
            if (!overlap)   circle_contours.push_back(circle_contour_candidates[i]);
        }
    }

    // check ellpisoid and make set of points //
    std::vector<cv::Point2f> source;
    std::vector<vector<array<int,3>>> circle_pts_candidates;
    circle_pts_candidates.resize(circle_contours.size());
    cv::Point2f pt;
    int W = img_origin.cols;
    int H = img_origin.rows;
    for (size_t i = 0; i < circle_contours.size(); i++)
    {
        std::vector<std::vector<cv::Point>> cnt;
        cnt.push_back(circle_contours[i]);
        cv::Mat mask = cv::Mat::zeros(img_origin.size(), CV_8UC1);
        cv::drawContours(mask, cnt, -1, cv::Scalar(255, 255, 255), cv::FILLED);

        std::vector<cv::Point> loc;
        cv::findNonZero(mask, loc); // output, locations of non-zero pixels

        for (int j = 0; j < loc.size(); j++)
        {
            int u = loc[j].x;
            int v = loc[j].y;
            int intensity = img_origin.data[v * W + u];
            circle_pts_candidates[i].push_back(array<int, 3>{u, v, intensity});
        }

        if (ellipse_test(circle_pts_candidates[i], pt))   
        {
            if(debug)
            {
                for(array<int,3> pnt: circle_pts_candidates[i])
                {
                    int pos = pnt[0] + pnt[1] * W;
                    img_output.data[3*pos] = img_output.data[3*pos] * 0.6 + 255 * 0.4;
                    img_output.data[3*pos+1]=img_output.data[3*pos] * 0.6;
                    img_output.data[3*pos+2]=img_output.data[3*pos] * 0.6;
                }  
            }

            std::cout << pt.x << " " << pt.y << std::endl;
            cv::Moments m_t = cv::moments(circle_contours[i]);
            pt.x = m_t.m10/m_t.m00;
            pt.y = m_t.m01/m_t.m00;
            std::cout << pt.x << " " << pt.y << std::endl << std::endl;
            source.push_back(pt);
        }
    }

    // check patten grid //
    // vector<cv::Point2f> center_pts;
    // std::vector<vector<array<int,3>>> circle_pts;
    // sortTarget(source, center_pts);
    // source = center_pts;
    // for (size_t i = 0; i < circle_contours.size(); i++)
    // {
    //     cv::Moments m = cv::moments(circle_contours[i]);
    //     if (m.m00 != 0)
    //     {
    //         cv::Point m_c = cv::Point(int(m.m10 / m.m00), int(m.m01 / m.m00));

    //         for (int j = 0; j < center_pts.size(); j++)
    //         {
    //             float dist = sqrt(pow(m_c.x - center_pts[j].x, 2) + pow(m_c.y - center_pts[j].y, 2));
    //             if (dist < 5.)
    //             {
    //                 circle_pts.push_back(circle_pts_candidates[i]);
    //                 break;
    //             }
    //         }
    //     }
    // }

    sortTarget(source,target);
    bool result=false;




    if(target.size()==n_x*n_y){
        result=true;
        if(debug){
            for(int i=0;i<target.size();i++)
            {
                cv::Point2f pt = target[i];
                for (int j=0;j<n_x;j++)
                {
                    if(i/n_x == j ) cv::putText(img_output,to_string(i%n_x),pt,0,1,text_colors[j],2);
                }
            }
        }
    }
    if(debug)
    {
        if(is_thermal) cv::resize(img_output, img_output, cv::Size(img_output.cols*1, img_output.rows*1));
        else cv::resize(img_output, img_output, cv::Size(img_output.cols*0.5, img_output.rows*0.5));
        cv::imshow("input_image",img_output);
    }
    
    return result;

}
bool TargetDetector::detect_circles(cv::Mat img, vector<cv::Point2f>&target, bool debug){
    int W = img.cols;
    int H = img.rows;

    cv::Mat bgr_img;
    if(draw) cv::cvtColor(img,bgr_img, cv::COLOR_GRAY2BGR);
    vector<cv::Point2f> source;

    // initialize
    vector<vector<bool>> buffer; // 확인 안했으면 true;
    buffer.reserve(H);
    for(int i=0;i<H;i++){
        vector<bool> temp(W,true);
        buffer.push_back(temp);
    }
    vector<array<int,3>> area;
    cv::Point2f pt;
    for(int y=0;y<H;y++){
        for(int x=0;x<W;x++){
            if(buffer[y][x] && check_pixel(img,x,y)){

                area.clear();
                dfs(img,buffer,area,x,y);
                if(ellipse_test(area,pt)){
                    if(debug){
                        for(int i=0;i<area.size();i++){
                            int pos= area[i][0]+area[i][1]*W;
                            if (use_weight) bgr_img.data[3*pos]=(color_threshold-area[i][2]);
                            else bgr_img.data[3*pos] = 100;
                            bgr_img.data[3*pos+1]=0;
                            bgr_img.data[3*pos+2]=0;
                        }
                    }
                    source.push_back(pt);
                }
            }
        }
    }

    // find axis
    if(debug){

        for(int i=0;i<target.size();i++)
        {
            cv::Point2f pt = target[i];
            for (int j=0;j<n_x;j++)
            {
                if(i/n_x == j ) cv::putText(bgr_img,to_string(i%n_x),pt,0,1,text_colors[j],2);
            }
        }
        if(is_thermal) cv::resize(bgr_img, bgr_img, cv::Size(bgr_img.cols*2, bgr_img.rows*2));
        else cv::resize(bgr_img, bgr_img, cv::Size(bgr_img.cols*0.8, bgr_img.rows*0.8));
        cv::imshow("input_image",bgr_img);
    }

    if(source.size()==n_x*n_y) {
        sortTarget(source,target);
        if(target.size()==n_x*n_y) {
            return  true;
        }
        else return false;
    }
    else return false;

}


void TargetDetector::sortTarget(vector<cv::Point2f>&source, vector<cv::Point2f>&dist){
    CircleGridFinder gridfinder(false);
    gridfinder.findGrid(source, cv::Size(n_x,n_y),dist);
    reverse(dist.begin(), dist.end());
    return;
}

