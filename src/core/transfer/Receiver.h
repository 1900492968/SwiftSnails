//
//  Receiver.h
//  SwiftSnails
//
//  Created by Chunwei on 3/09/15.
//  Copyright (c) 2015 Chunwei. All rights reserved.
//
#ifndef Swift_transfer_Receiver_h_
#define Swift_transfer_Receiver_h_
#include "../../utils/common.h"
#include "../AsynExec.h"
#include "../../utils/SpinLock.h"
#include "Listener.h"
#include "ListenService.h"
namespace swift_snails {

template<typename Handler>
class MessageClass :public VirtualObject {
public:

    void add (index_t id, Handler&& handler) {
        LOG(INFO) << "register message class: " << id;
        std::lock_guard<SpinLock> lock(_spinlock);
        is_writing = true;
        CHECK(_message_classes.count(id) == 0) <<
                "callback should be registerd only once";
        _message_classes.emplace(id, std::move(handler));
        is_writing = false;
    }

    void remove (index_t id) {
        std::lock_guard<SpinLock> lock(_spinlock);
        is_writing = true;
        auto pos = _message_classes.find(id);
        CHECK(pos != _message_classes.end()) <<
                "no message_class:" << id << " found!";
        _message_classes.erase(pos);
        is_writing = false;
    }

    Handler& get (index_t id) {
        auto it = _message_classes.find(id); 
        CHECK(it != _message_classes.end());
        return it->second;
    }

    const std::map<index_t, Handler>& 
    message_classes() {
        return _message_classes;
    }

private:
    SpinLock _spinlock;
    std::map<index_t, Handler> _message_classes;
    bool is_writing = false;
    //std::condition_variable _cond;
    //std::mutex _mut;
};


class Receiver : public Listener {

public:
    typedef std::function<void(std::shared_ptr<Request>)> handler_t;

    explicit Receiver(BaseRoute& route) :
        Listener(route.zmq_ctx()),
        _route_ptr(&route) 
    {
        //PCHECK(_receiver = zmq_socket(_zmq_ctx, ZMQ_PULL));
    }

    void set_async_channel(std::shared_ptr<AsynExec::channel_t> channel) {
        _async_channel = channel;
    }

    void send_response(Request &&request, int to_id) {
        Package package(request);
        BaseRoute& route = *_route_ptr;
        {
            // TODO will the mutex share between sender and receiver 
            // effect performance?
            std::lock_guard<std::mutex> lock(
                * route.send_mutexes()[to_id]
            );
            PCHECK(ignore_signal_call(zmq_msg_send, &package.meta.zmg(), route.senders()[to_id], ZMQ_SNDMORE) >= 0);
            PCHECK(ignore_signal_call(zmq_msg_send, &package.cont.zmg(), route.senders()[to_id], 0) >= 0);
        }
    }

    void main_loop() override {
        for(;;) {
            Package package;
            { std::lock_guard<std::mutex> lock(receiver_mutex() );
                // receive meta-message first
                PCHECK(ignore_signal_call(zmq_msg_recv, &package.meta.zmg(), receiver(), 0) >= 0);
                LOG(INFO) << "receive a message";
                if(package.meta.size() == 0) break; // exit server
                CHECK(package.meta.size() == sizeof(MetaMessage));
                CHECK(zmq_msg_more(&package.meta.zmg()));
                // receive content-message later
                PCHECK(ignore_signal_call(zmq_msg_recv, &package.cont.zmg(), receiver(), 0) >= 0);
                CHECK(!zmq_msg_more(&package.cont.zmg()));
            }
            auto request = std::make_shared<Request>(std::move(package));
            handler_t handler = _message_class.get( request->meta.message_class);
            _async_channel->push(
                [&handler, request] {
                    handler(request);
                }
            );
        }
        LOG(WARNING) << "server terminated!";
    }

    bool service_complete() override {
        return true;
    }

    /*
     * work as an API
     *
     * control Receiver service by adding handler to message_class
     */
    MessageClass<handler_t>& message_class() {
        return _message_class;
    }


private:

    BaseRoute* _route_ptr;
    MessageClass<handler_t> _message_class;
    std::shared_ptr<AsynExec::channel_t> _async_channel;
}; // end class Receiver

}; // end namespace
#endif
