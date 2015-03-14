//
//  Sender.h
//  SwiftSnails
//
//  Created by Chunwei on 3/09/15.
//  Copyright (c) 2015 Chunwei. All rights reserved.
//
#ifndef Swift_transfer_Sender_h_
#define Swift_transfer_Sender_h_
#include "../../utils/common.h"
#include "../AsynExec.h"
#include "../../utils/SpinLock.h"
#include "Listener.h"
namespace swift_snails {

class Sender : public Listener {
public:
    Sender(BaseRoute &route) :
        Listener(route.zmq_ctx()),
        _route_ptr(&route)
    {
        //PCHECK(_receiver = zmq_socket(_zmq_ctx, ZMQ_PULL));
    }

    void set_async_channel(std::shared_ptr<AsynExec::channel_t> channel) {
        _async_channel = channel;
    }

    void set_client_id(int client_id) {
        _client_id = client_id;
    }
    /*
     * @request:    request
     * @to_id:  id of the node where the message is sent to
     */
    void send(Request &&request, int to_id) {
        index_t msg_id = _msg_id_counter++;
        request.set_msg_id(msg_id);
        CHECK(_client_id != 0) << "shoud set client_id first";
        request.meta.client_id = _client_id;
        // convert Request to underlying Package
        Package package(request);
        // cache the recall_back
        // when the sent message's reply is received 
        // the call_back handler will be called
        { std::lock_guard<SpinLock> lock(_send_mut);
            CHECK(_msg_handlers.emplace(msg_id, std::move(request.call_back_handler)).second);
        }

        // send the package
        BaseRoute& route = *_route_ptr;

        {
            std::lock_guard<std::mutex> lock(
                * route.send_mutex(to_id)
            );
            PCHECK(ignore_signal_call(zmq_msg_send, &package.meta.zmg(), route.sender(to_id), ZMQ_SNDMORE) >= 0);
            PCHECK(ignore_signal_call(zmq_msg_send, &package.cont.zmg(), route.sender(to_id), 0) >= 0);
        }
    }

    /*
     * should run as a thread
     * receive reply message, read the reply message and run the correspondding handler
     */
    void main_loop() {
        Package package;
        Request::response_call_back_t handler;
        for(;;) {

            { std::lock_guard<std::mutex> lock(receiver_mutex() );
                PCHECK(ignore_signal_call(zmq_msg_recv, &package.meta.zmg(), receiver(), 0) >= 0);
                if(package.meta.size() == 0) break;
                CHECK(zmq_msg_more(&package.meta.zmg()));
                PCHECK(ignore_signal_call(zmq_msg_recv, &package.cont.zmg(), receiver(), 0) >= 0);
                CHECK(!zmq_msg_more(&package.cont.zmg()));
            }

            std::shared_ptr<Request> response = std::make_shared<Request>(std::move(package));
            LOG(INFO) << "receive a response, message_id: " << response->meta.message_id;

            CHECK(_client_id == 0 || response->meta.client_id == client_id());
            //LOG(INFO) << ".. call response handler";
            // call the callback handler
            { std::lock_guard<SpinLock> lock(_msg_handlers_mut);
                auto it = _msg_handlers.find(response->message_id());
                CHECK(it != _msg_handlers.end());
                handler = std::move(it->second);
                _msg_handlers.erase(it);
            }

            //LOG(INFO) << ".. push response handler to channel";

            // execute the response_recallback handler
            _async_channel->push(
                // TODO refrence handler?
                [handler, this, response]() {
                    handler(response);
                }
            );
        }
        LOG(WARNING) << "sender terminated!";
    }

    int client_id() const {
        return _client_id;
    }

    // determine whether all sended message get a 
    // reply
    // Attention: not thread safe!
    bool service_complete() {
        return _msg_handlers.empty();
    }

    ~Sender() {
        CHECK(service_complete());
    }

private:

    BaseRoute*  _route_ptr;

    index_t _msg_id_counter = 0;
    std::shared_ptr<AsynExec::channel_t> _async_channel;
    std::map<index_t, Request::response_call_back_t> _msg_handlers;

    SpinLock    _send_mut;
    SpinLock    _msg_handlers_mut;
    //SpinLock    _response_cache_mut;

    // cache response until it is handled
    //std::map<index_t, Request> _responses;

    int _client_id = 0;
}; // end class 



}; // end namespace
#endif
