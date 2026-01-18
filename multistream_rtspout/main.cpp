#include <iostream>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h> 
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#define num_sources 4

const char *uri_list =
        "file:///workspace/data/test.mp4,"
        "file:///workspace/data/test.mp4,"
        "file:///workspace/data/test.mp4,"
        "file:///workspace/data/test.mp4";

//CALLBACK(runs once per stream) for demux
static void on_demux_pad_added(GstElement *demux, GstPad *pad, gpointer data){
    const gchar *padname = GST_PAD_NAME(pad);
    const char *underscore_ptr = strrchr(padname, '_');
    
    if(!underscore_ptr){
        std::cout << "Padname error. Exiting." << std::endl;
        return;
    }

    int stream_id = atoi(underscore_ptr + 1);
    int udp_port = 5400 + stream_id; 

    //Make sure the input is video only
    GstCaps *pad_caps = gst_pad_query_caps(pad, NULL);
    if(!pad_caps){
        std::cout << "Failed to query pad_caps. Exiting." << std::endl;
        return;
    }

    GstStructure *str = gst_caps_get_structure(pad_caps, 0);
    const char *name = gst_structure_get_name(str);
    
    if(!g_str_has_prefix(name, "video")){
        std::cout << "Input stream does not contain a video. Exiting." << std::endl;
        gst_caps_unref(pad_caps);
        return;
    }

    gst_caps_unref(pad_caps);

    //Creating elements for pipeline branches
    GstElement *pipeline = (GstElement *)data;
    GstElement *queue = gst_element_factory_make("queue", NULL);
    GstElement *sink = gst_element_factory_make("udpsink", NULL);
    GstElement *capsfilter1 = gst_element_factory_make("capsfilter", NULL);
    GstElement *capsfilter2 = gst_element_factory_make("capsfilter", NULL);
    GstElement *nvdsosd = gst_element_factory_make("nvdsosd", NULL);
    GstElement *conv1 = gst_element_factory_make("nvvideoconvert", NULL);
    GstElement *conv2 = gst_element_factory_make("nvvideoconvert", NULL);
    GstElement *encoder = gst_element_factory_make("nvv4l2h264enc", NULL);
    GstElement *rtp = gst_element_factory_make("rtph264pay", NULL);

    if(!pipeline || !queue || !sink || !capsfilter1 || !capsfilter2 || !nvdsosd || !conv1 || !conv2 || !encoder || !rtp){
        std::cout << "Failed to create one of the elements in the callback. Exiting" << std::endl;
        return;
    }

    GstCaps *caps1 = gst_caps_from_string("video/x-raw(memory:NVMM), format=RGBA");
    GstCaps *caps2 = gst_caps_from_string("video/x-raw(memory:NVMM), format=NV12");
    if(!caps1 || !caps2){
        std::cout << "Failed to create caps from string. Exiting." << std::endl;
        return;
    }

    //Configure Elements
    g_object_set(G_OBJECT(capsfilter1), "caps", caps1, NULL);
    g_object_set(G_OBJECT(capsfilter2), "caps", caps2, NULL);
    
    g_object_set(G_OBJECT(nvdsosd), "display-clock", TRUE, NULL);
    g_object_set(G_OBJECT(sink), "sync", TRUE, "async", FALSE,"port", udp_port, "host", "127.0.0.1", NULL);
    g_object_set(G_OBJECT(rtp), "pt", 96, "config-interval", 1, NULL);
    g_object_set(G_OBJECT(encoder), "bitrate", 4000000, "insert-sps-pps", 1, "bufapi-version", 1, NULL);

    gst_caps_unref(caps1);
    gst_caps_unref(caps2);

    //Add and link elements
    gst_bin_add_many(GST_BIN(pipeline), queue, conv1, capsfilter1, nvdsosd, conv2, capsfilter2, encoder, rtp, sink, NULL);
    if(!gst_element_link_many(queue, conv1, capsfilter1, nvdsosd, conv2, capsfilter2, encoder, rtp, sink, NULL)){
        std::cout << "Failed to link elements. Exiting." << std::endl;
        return;
    }
    
    //Linking demux to the queue
    GstPad *sinkpad = gst_element_get_static_pad(queue, "sink");
    if(gst_pad_link(pad, sinkpad) != GST_PAD_LINK_OK){
        std::cout << "Failed to link demux to branch at source id: " << stream_id << ". Exiting." << std::endl;
        gst_object_unref(sinkpad);
        return;
    }

    gst_object_unref(sinkpad);

    //Resetting elements
    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(sink);
    gst_element_sync_state_with_parent(nvdsosd);
    gst_element_sync_state_with_parent(conv1);  
    gst_element_sync_state_with_parent(conv2);
    gst_element_sync_state_with_parent(capsfilter1);
    gst_element_sync_state_with_parent(capsfilter2);
    gst_element_sync_state_with_parent(encoder);
    gst_element_sync_state_with_parent(rtp); 
}

//FUNCTION TO GENERATE GRAPH BEFORE RUNNING LOOP
static gboolean graph_generation(gpointer data){
    GstElement *pipeline = (GstElement *)data;

    GST_DEBUG_BIN_TO_DOT_FILE(
        GST_BIN(pipeline),
        GST_DEBUG_GRAPH_SHOW_ALL,
        "pipeline_playing"
    );

    return FALSE;
}

int main(int argc, char *argv[]){
    GstElement *pipeline = NULL;
    GstElement *pgie = NULL;
    GstElement *demux = NULL;
    GstElement *multisrc = NULL;

    GMainLoop *loop = NULL;

    gst_init(&argc, &argv);
    loop = g_main_loop_new(NULL, FALSE); 

    GstRTSPServer *server = gst_rtsp_server_new();
    g_object_set(server, "service", "8554", NULL); 

    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);

    for (int i = 0; i < num_sources; i++) {
        char path[64];
        char udp_launch_str[512];
        int udp_port = 5400 + i;

        sprintf(path, "/stream%d", i);
        
        sprintf(udp_launch_str, 
            "( udpsrc port=%d caps=\"application/x-rtp, media=video, clock-rate=90000, encoding-name=H264, payload=96\" ! "
            "rtph264depay ! rtph264pay name=pay0 )", 
            udp_port);
        GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
        gst_rtsp_media_factory_set_launch(factory, udp_launch_str);
        gst_rtsp_media_factory_set_shared(factory, TRUE);

        gst_rtsp_mount_points_add_factory(mounts, path, factory);
        
        std::cout << "RTSP Stream ready at rtsp://127.0.0.1:8554" << path << std::endl;
    }
    
    g_object_unref(mounts);
    gst_rtsp_server_attach(server, NULL);

    pipeline = gst_pipeline_new("demux-pipeline");
    multisrc = gst_element_factory_make("nvmultiurisrcbin", "multi-src");
    pgie = gst_element_factory_make("nvinfer", "pgie");
    demux = gst_element_factory_make("nvstreamdemux", "demux");

    if(!pipeline || !multisrc || !pgie || !demux){
        std::cout << "Failed to create elements." << std::endl;
        return -1;
    }

    //configure elements
    gst_element_set_state(multisrc, GST_STATE_NULL);
    g_object_set(G_OBJECT(multisrc), "width", 1280, "height", 720, "enable-padding", 1, NULL);
    g_object_set(G_OBJECT(multisrc), "batched-push-timeout", 400000, "uri-list", uri_list, NULL);
    g_object_set(G_OBJECT(multisrc), "file-loop", TRUE, NULL);
    g_object_set(G_OBJECT(pgie), "config-file-path", "/workspace/data/multi/config_infer_primary_yolov8.txt", NULL);
    
    gst_bin_add_many(GST_BIN(pipeline), multisrc, pgie, demux, NULL);
    if(!gst_element_link_many(multisrc, pgie, demux, NULL)){
        std::cout << "Failed to connect nvinfer to demuxer." << std::endl;
        return -1;
    }

    g_signal_connect(demux, "pad-added", G_CALLBACK(on_demux_pad_added), pipeline);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    //Generate graph after 3 seconds
    g_timeout_add_seconds(3, graph_generation, pipeline);

    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_main_loop_unref(loop);
    gst_object_unref(pipeline);

    return 0;
}
