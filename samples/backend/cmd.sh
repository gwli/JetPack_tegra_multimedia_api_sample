data_dir="/home/nvidia/tegra_multimedia_api"
./backend 1 $data_dir/data/Video/sample_outdoor_car_1080p_10fps.h264 H264 --trt-deployfile $data_dir/data/Model/GoogleNet_one_class/GoogleNet_modified_oneClass_halfHD.prototxt --trt-modelfile $data_dir/data/Model/GoogleNet_one_class/GoogleNet_modified_oneClass_halfHD.caffemodel --trt-forcefp32 0 --trt-proc-interval 1 -fps 30
