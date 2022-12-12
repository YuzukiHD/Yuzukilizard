/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CedarXMetaData"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <string.h>

#include <MetaData.h>
#include <CedarXMetaData.h>

namespace EyeseeLinux {

MetaData::MetaData() {
}

MetaData::MetaData(const MetaData &from)
    : mItems(from.mItems) 
{
}

MetaData::~MetaData() 
{
    clear();
}

void MetaData::clear() 
{
    mItems.clear();
}

bool MetaData::remove(uint32_t key) 
{
    size_t cnt = mItems.erase(key);
    if(cnt <= 0)
    {
        return false;
    }
    return true;
}

bool MetaData::setCString(uint32_t key, const char *value) 
{
    return setData(key, TYPE_C_STRING, value, strlen(value) + 1);
}

bool MetaData::setInt32(uint32_t key, int32_t value) 
{
    return setData(key, TYPE_INT32, &value, sizeof(value));
}

bool MetaData::setInt64(uint32_t key, int64_t value) 
{
    return setData(key, TYPE_INT64, &value, sizeof(value));
}

bool MetaData::setFloat(uint32_t key, float value) 
{
    return setData(key, TYPE_FLOAT, &value, sizeof(value));
}

bool MetaData::setPointer(uint32_t key, void *value) 
{
    return setData(key, TYPE_POINTER, &value, sizeof(value));
}

bool MetaData::findCString(uint32_t key, const char **value) 
{
    uint32_t type;
    const void *data;
    size_t size;
    if (!findData(key, &type, &data, &size) || type != TYPE_C_STRING) 
    {
        return false;
    }

    *value = (const char *)data;

    return true;
}

bool MetaData::findInt32(uint32_t key, int32_t *value) 
{
    uint32_t type;
    const void *data;
    size_t size;
    if (!findData(key, &type, &data, &size) || type != TYPE_INT32) 
    {
        return false;
    }

    if(!(size==sizeof(*value)))
    {
        aloge("fatal error, INT32 size is not match");
    }

    *value = *(int32_t *)data;

    return true;
}

bool MetaData::findInt64(uint32_t key, int64_t *value) 
{
    uint32_t type;
    const void *data;
    size_t size;
    if (!findData(key, &type, &data, &size) || type != TYPE_INT64) 
    {
        return false;
    }

    if(!(size==sizeof(*value)))
    {
        aloge("fatal error, INT64 size is not match");
    }

    *value = *(int64_t *)data;

    return true;
}

bool MetaData::findFloat(uint32_t key, float *value) 
{
    uint32_t type;
    const void *data;
    size_t size;
    if (!findData(key, &type, &data, &size) || type != TYPE_FLOAT) 
    {
        return false;
    }

    if(!(size==sizeof(*value)))
    {
        aloge("fatal error, FLOAT size is not match");
    }

    *value = *(float *)data;

    return true;
}

bool MetaData::findPointer(uint32_t key, void **value) 
{
    uint32_t type;
    const void *data;
    size_t size;
    if (!findData(key, &type, &data, &size) || type != TYPE_POINTER) 
    {
        return false;
    }

    if(!(size==sizeof(*value)))
    {
        aloge("fatal error, POINTER size is not match");
    }

    *value = *(void **)data;

    return true;
}

bool MetaData::setData(
        uint32_t key, uint32_t type, const void *data, size_t size) 
{
    bool overwrote_existing = true;
    std::map<uint32_t, typed_data>::iterator it = mItems.find(key);
    if (it == mItems.end())
    {
        overwrote_existing = false;
    }
    mItems[key].setData(type, data, size);
    return overwrote_existing;
}

bool MetaData::findData(uint32_t key, uint32_t *type,
                        const void **data, size_t *size) const 
{
    std::map<uint32_t, typed_data>::const_iterator it = mItems.find(key);
    if (it == mItems.end())
    {
        return false;
    }
    //mItems[key].getData(type, data, size);
    it->second.getData(type, data, size);
    return true;
}

MetaData::typed_data::typed_data()
    : mType(0),
      mSize(0) 
{
}

MetaData::typed_data::~typed_data() 
{
    clear();
}

MetaData::typed_data::typed_data(const typed_data &from)
    : mType(from.mType),
      mSize(0) 
{
    allocateStorage(from.mSize);
    memcpy(storage(), from.storage(), mSize);
}

MetaData::typed_data &MetaData::typed_data::operator=(
        const MetaData::typed_data &from) 
{
    if (this != &from) 
    {
        clear();
        mType = from.mType;
        allocateStorage(from.mSize);
        memcpy(storage(), from.storage(), mSize);
    }

    return *this;
}

void MetaData::typed_data::clear() 
{
    freeStorage();

    mType = 0;
}

void MetaData::typed_data::setData(
        uint32_t type, const void *data, size_t size) 
{
    clear();

    mType = type;
    allocateStorage(size);
    memcpy(storage(), data, size);
}

void MetaData::typed_data::getData(
        uint32_t *type, const void **data, size_t *size) const 
{
    *type = mType;
    *size = mSize;
    *data = storage();
}

void MetaData::typed_data::allocateStorage(size_t size) 
{
    mSize = size;

    if (usesReservoir()) 
    {
        return;
    }

    u.ext_data = malloc(mSize);
}

void MetaData::typed_data::freeStorage() 
{
    if (!usesReservoir()) 
    {
        if (u.ext_data) 
        {
            free(u.ext_data);
        }
    }

    mSize = 0;
}

}  // namespace android

