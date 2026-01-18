#include <iostream>
#include <gst/gst.h>
#include <glib.h>
#include <string>
#include <nvdsmeta.h>
#include <nvdsmeta_schema.h>

#define num_sources 4

static gboolean graph_generation(gpointer data){
    GstElement *pipeline = (GstElement *)data;

    GST_DEBUG_BIN_TO_DOT_FILE(
        GST_BIN(pipeline),
        GST_DEBUG_GRAPH_SHOW_ALL,
        "pipeline_playing"
    );

    return FALSE;
}

static void cb_newpad(GstElement *src, GstPad *new_pad, gpointer data){
    GstElement *streammux = (GstElement *)data;
    GstPad *sinkpad = gst_element_get_request_pad(streammux, "sink_0");

    if(!sinkpad){
        std::cout << "Failed to get pad for sink. Exiting." << std::endl;
        return;
    }

    if(gst_pad_is_linked(sinkpad)){
        std::cout << "Pad linked already " << std::endl;
        gst_object_unref(sinkpad);
        return;
    }

    if(gst_pad_link(new_pad, sinkpad) != GST_PAD_LINK_OK){
        std::cout << "Failed to link new pad. Exiting." << std::endl;
    }

    gst_object_unref(sinkpad);
}

int main(int argc, char *argv[]){
    //GMainLoop *loop = NULL;

    //initialize Gstreamer
    gst_init(&argc, &argv);
    //loop = g_main_loop_new(NULL, FALSE);

    GstElement *pipeline= gst_pipeline_new("ds-pipeline");
    GstElement *source= gst_element_factory_make("nvurisrcbin", "source");
    GstElement *streammux= gst_element_factory_make("nvstreammux", "stream-muxer");
    GstElement *pgie= gst_element_factory_make("nvinfer", "infer");
    GstElement *tiler = gst_element_factory_make("nvmultistreamtiler", "stream-tiler");
    GstElement *nvosd= gst_element_factory_make("nvdsosd", "onscreen-display");
    //file-sink for testing
    GstElement *sink= gst_element_factory_make("filesink", "sink");
    GstElement *vidconv = gst_element_factory_make("nvvideoconvert", "video-converter");
    GstElement *parser = gst_element_factory_make("h264parse", "h264-parser");
    GstElement *encoder = gst_element_factory_make("nvv4l2h264enc", "h264-encoder");
    GstElement *muxer = gst_element_factory_make("qtmux", "muxer");

    if(!pipeline || !source || !streammux || !pgie || !tiler || !nvosd || !sink || !vidconv || !parser || !encoder || !muxer){
        std::cout << "One of the elements could not be created. Exiting." << std::endl;
        return -1;
    }

    //configure elements
    g_object_set(G_OBJECT(source), "uri", "file:///workspace/data/test.mp4", NULL);
    g_object_set(G_OBJECT(streammux), "width", 1280, "height", 720, "batch-size", 1, "batched-push-timeout", 4000000,"live-source", 0, NULL);
    g_object_set(G_OBJECT(pgie), "config-file-path", "/workspace/data/basic_pipeline/config_infer_primary_yolov8.txt", NULL);
    g_object_set(G_OBJECT(nvosd), "process-mode", 1, NULL);
    g_object_set(G_OBJECT(nvosd), "display-text", 1, NULL);
    g_object_set(G_OBJECT(nvosd), "font-size", 12, NULL);
    g_object_set(G_OBJECT(nvosd), "gpu-id", 0, NULL);
    g_object_set(G_OBJECT(nvosd), "show-clock", 1,"clock-x-offset", 10, "clock-y-offset", 10,"clock-text-size", 12,"clock-color", 0xFFFF0000, NULL);
    g_object_set(G_OBJECT(tiler),"rows", 1,"columns", 1,"width", 1280,"height", 720, NULL);
    g_object_set(G_OBJECT(encoder), "bitrate", 4000000, "preset-level", 1, "insert-sps-pps", 1, "bufapi-version", 1, NULL);
    g_object_set(G_OBJECT(sink), "location", "/workspace/data/output.mp4","sync", FALSE, NULL);
    //Add elements to pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, streammux, pgie, tiler, nvosd, vidconv, encoder, parser, muxer, sink, NULL);
    
    //creating capsfilter to make sure the encoder gets the right format(NV12)
    GstCaps *caps = gst_caps_from_string("video/x-raw(memory:NVMM), format=NV12");
    GstElement *capsfilter = gst_element_factory_make("capsfilter", "caps");

    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_bin_add(GST_BIN(pipeline), capsfilter);

    if(!gst_element_link_many(streammux, pgie, tiler, nvosd, vidconv, capsfilter, encoder, parser, muxer, sink, NULL)){
        std::cout << "Elements could not be linked. Exiting." << std::endl;
        gst_object_unref(pipeline);
        return -1;
    }
    //Start pipeline

    g_signal_connect(source, "pad-added", G_CALLBACK(cb_newpad), streammux);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    g_timeout_add_seconds(5, graph_generator, pipeline);

    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    //Free resources
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(bus));
    gst_object_unref(GST_OBJECT(pipeline));

    return 0;
}   