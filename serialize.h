#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <string.h>
#include <string>
#include <fstream>

namespace serialize {
    template <class Archive, class T>
    void serialize(Archive& ar, T& object) {
        static_assert(!std::is_class_v<T> || std::is_same_v<T, std::string>, "serialize not support class type");
        ar& object;
    }
} // namespace serialize

namespace serialize {
    template <typename iostream>
    class Archive {
        struct Store {
            char* buffer = nullptr;
            size_t* bufsize = nullptr;
            size_t capacity = 0;
            size_t offset = 0;
            void append(void* buf, size_t size) {
                memcpy(buffer + *bufsize, buf, size);
                *bufsize += size;
            }
            template <class T>
            void popend(T&& v, size_t len, std::ifstream* ifs = nullptr, size_t total = 0) {
                if (offset + len > *bufsize) {
                    if (!ifs || !ifs->is_open())
                        throw "pop overflow";
                    memcpy(v + total, buffer + offset, *bufsize - offset);
                    total += *bufsize - offset;
                    *bufsize = ifs->read(buffer, capacity).gcount();
                    if (*bufsize == 0)
                        throw "pop overflow";
                    offset = 0;
                    return popend(v, len - total, ifs, total);
                }
                memcpy(v + total, buffer + offset, len);
                offset += len;
                total += len;
                len -= total;
            }
        };
    protected:
        char cache[40960] = { 0 };
        size_t cachesize = 0;
        char* data = 0;
        size_t size = 0;
        size_t capacity = 0;
        Store store = { cache, &cachesize, sizeof(cache) };
        iostream io;
        Archive() = default;
        void check_memory(size_t len) {
            if (len + cachesize <= sizeof(cache))
                return;
            len += size;
            if (len > capacity) {
                capacity = len + len / 2;
                void* buf = realloc(data, capacity);
                if (!buf)
                    throw "realloc failed";
                data = (char*)buf;
            }
            if (store.buffer != data) {
                store.buffer = data;
                store.bufsize = &size;
            }
        }
        template <class T>
        void append(T v) {
            int len = sizeof(v);
            check_memory(4 + len);
            store.append(&len, 4);
            store.append(&v, len);
        }
        void append(const char* v, int len) {
            check_memory(4 + len);
            store.append(&len, 4);
            store.append((void*)v, len);
        }
        void append(const char* v) {
            append(v, strlen(v));
        }
        void append(const std::string& v) {
            append(v.c_str(), v.size());
        }
    public:
        Archive(const std::string& file) {
            auto mode = std::is_same_v<iostream, std::ifstream> ?
                std::ios::in | std::ios::binary :
                std::ios::out | std::ios::binary;
            if (!io.is_open())
                io.open(file, mode);
        }
        size_t len() {
            return size;
        }
        void clear() {
            cachesize = 0;
            if (data)
                free(data);
            data = 0;
            size = 0;
            capacity = 0;
            if (io.is_open())
                io.close();
        }
        void flush() {
            if (io.is_open() && std::is_same_v<iostream, std::ofstream>) {
                if (cachesize)
                    io.write(cache, cachesize);
                if (size)
                    io.write(data, size);
                io.flush();
                size = 0;
            }
        }
    };
    class IArchive;
    class OArchive;
    class IArchive final : public Archive<std::ofstream> {
        friend class OArchive;
    public:
        IArchive() = default;
        using Archive::Archive;
        const char* get() {
            return data;
        }
        ~IArchive() {
            clear();
        }
        template <class T>
        IArchive& operator&(const T& v) {
            std::decay_t<T> val = v;
            serialize(*this, val);
            return *this;
        }
        template <class T>
        IArchive& operator&(T& v) {
            if (std::is_class<T>::value) {
                std::decay_t<T> val = v;
                serialize(*this, val);
                return *this;
            }
            append(v);
            return *this;
        }
        IArchive& operator&(const char* v) {
            append(v, strlen(v));
            return *this;
        }
        IArchive& operator&(char* v) {
            append(v, strlen(v));
            return *this;
        }
        IArchive& operator&(std::string& v) {
            append(v);
            return *this;
        }
        IArchive& operator&(const std::string& v) {
            append(v);
            return *this;
        }
        template <class T1, class T2>
        IArchive& operator&(std::pair<T1, T2>& v) {
            operator&(v.first);
            operator&(v.second);
            return *this;
        }
        template <class T>
        IArchive& operator&(std::vector<T>& vec) {
            check_memory(4);
            int len = vec.size();
            store.append(&len, 4);
            for (auto ite = vec.begin(); ite != vec.end(); ite++)
                serialize(*this, *ite);
            return *this;
        }

        template <class T>
        IArchive& operator&(std::set<T>& vec) {
            check_memory(4);
            int len = vec.size();
            store.append(&len, 4);
            for (auto ite = vec.begin(); ite != vec.end(); ite++)
                serialize(*this, *ite);
            return *this;
        }
        template <class T, class V>
        IArchive& operator&(std::map<T, V>& object) {
            check_memory(4);
            int len = object.size();
            store.append(&len, 4);
            for (auto ite = object.begin(); ite != object.end(); ite++) {
                serialize(*this, ite->first);
                serialize(*this, ite->second);
            }
            return *this;
        }
    };

    class OArchive final : public Archive<std::ifstream> {
        friend class IArchive;
        void copy(const char* buf, size_t len) {
            if (len + cachesize <= sizeof(cache)) {
                memcpy(cache, buf, len);
                cachesize += len;
                return;
            }
            this->data = (char*)malloc(len);
            memcpy(data, buf, len);
            this->size = len;
            this->capacity = len;
        }
    public:
        OArchive() = default;
        using Archive::Archive;
        explicit OArchive(const char* buf, size_t len) {
            copy(buf, len);
        }
        OArchive(const IArchive& iar) {
            copy(iar.cache, iar.cachesize);
            copy(iar.data, iar.size);
        }
        ~OArchive() {
            clear();
        }
        void move(OArchive& oar) {
            memcpy(cache, oar.cache, oar.cachesize);
            this->data = oar.data;
            this->size = oar.size;
            this->capacity = oar.capacity;
            oar.data = nullptr;
            oar.clear();
        }
        OArchive(const OArchive& oar) {
            copy(oar.data, oar.size);
        }
        void clear() {
            this->cachesize = 0;
            if (data)
                free(data);
            this->data = 0;
            this->size = 0;
            this->capacity = 0;
        }
        int realsize() {
            int len = 0;
            if (io.is_open()) {
                store.popend(&len, 4, &io);
                return len;
                /*io.read((char*)&len, 4);
                if (len == 0)
                    throw "overflow";
                return len;*/
            }
            if (store.buffer != data && store.offset >= cachesize) {
                store.buffer = data;
                store.bufsize = &size;
                store.offset = 0;
            }
            if (store.offset + 4 > cachesize + size)
                throw "overflow";
            store.popend(&len, 4, &io);
            if (store.offset + len > cachesize + size)
                throw "overflow";
            return len;
        }
        template <class T>
        OArchive& operator&(T& v) {
            if (std::is_class<T>::value) {
                serialize(*this, v);
                return *this;
            }
            int len = realsize();
            store.popend(&v, len, &io);
            return *this;
        }
        OArchive& operator&(char*& v) {
            int len = realsize();
            v = (char*)malloc(len);
            store.popend(v, len, &io);
            return *this;
        }
        OArchive& operator&(std::string& v) {
            int len = realsize();
            char* buf = (char*)malloc(len);
            store.popend(buf, len, &io);
            v = std::string(buf, len);
            free(buf);
            return *this;
        }

        template <class T1, class T2>
        OArchive& operator&(std::pair<T1, T2>& v) {
            T1 v1;
            T2 v2;
            operator&(v1);
            operator&(v2);
            v.first = v1;
            v.second = v2;
            return *this;
        }
        template <class T>
        OArchive& operator&(std::vector<T>& v) {
            int len = realsize();
            for (int i = 0; i < len; i++) {
                T item;
                operator&(item);
                v.push_back(item);
            }
            return *this;
        }
        template <class T>
        OArchive& operator&(std::set<T>& v) {
            int len = realsize();
            for (int i = 0; i < len; i++) {
                T item;
                operator&(item);
                v.insert(item);
            }
            return *this;
        }
        template <class K, class V>
        OArchive& operator&(std::map<K, V>& object) {
            int len = realsize();
            for (int i = 0; i < len; i++) {
                K k;
                V v;
                operator&(k);
                operator&(v);
                object[k] = v;
            }
            return *this;
        }
    };
} // namespace serialize
