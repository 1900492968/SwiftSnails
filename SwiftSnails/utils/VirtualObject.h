//
//  VirtualObject.h
//  SwiftSnails
//
//  Created by Chunwei on 12/2/14.
//  Copyright (c) 2014 Chunwei. All rights reserved.
//

#ifndef SwiftSnails_VirtualObject_h
#define SwiftSnails_VirtualObject_h
namespace SwiftSnails {

class VirtualObject {
    VirtualObject() = default;
    VirtualObject(const VirtualObject &) = delete;
    VirtualObject &operator= (const VirtualObject &) = delete;
    ~VirtualObject() = default;
};



}; // end namespace SwiftSnails


#endif
