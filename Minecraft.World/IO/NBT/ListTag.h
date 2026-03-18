#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include "Tag.h"

template <class T>
class ListTag : public Tag {
private:
    std::vector<Tag*> list;
    uint8_t type;

public:
    ListTag() : Tag(L""), type(1) {}
    ListTag(const std::wstring& name) : Tag(name), type(1) {}

    void write(DataOutput* dos) override {
        if (!list.empty())
            type = list[0]->getId();
        else
            type = static_cast<uint8_t>(1);

        dos->writeByte(type);
        dos->writeInt(static_cast<int>(list.size()));

        for (auto it = list.begin(); it != list.end(); ++it) {
            (*it)->write(dos);
        }
    }

    void load(DataInput* dis) override {
        type = dis->readByte();
        int size = dis->readInt();

        list.clear();
        for (int i = 0; i < size; i++) {
            Tag* tag = Tag::newTag(type, L"");
            tag->load(dis);
            list.push_back(tag);
        }
    }

    uint8_t getId() override { return TAG_List; }

    std::wstring toString() override {
        static wchar_t buf[128];
        // Using %zu for size_t consistency on 64-bit. <- we'll prob need to
        // change this for other platforms
        swprintf(buf, 128, L"%zu entries of type %ls", list.size(),
                 Tag::getTagName(type));
        return std::wstring(buf);
    }

    void print(const char* prefix, std::ostream& out) {
        out << prefix << Tag::getTagName(getId()) << "(\"" << getName()
            << "\"): " << list.size() << " entries of type "
            << Tag::getTagName(type) << std::endl;
        out << prefix << "{" << std::endl;

        size_t prefixLen = strlen(prefix);
        char* newPrefix = new char[prefixLen + 4];
        strcpy(newPrefix, prefix);
        strcat(newPrefix, "   ");

        for (auto it = list.begin(); it != list.end(); ++it) {
            // (*it)->print(newPrefix, out);
        }

        delete[] newPrefix;
        out << prefix << "}" << std::endl;
    }

    void add(T* tag) {
        type = tag->getId();
        list.push_back(tag);
    }

    T* get(int index) { return static_cast<T*>(list[index]); }

    int size() const { return static_cast<int>(list.size()); }

    virtual ~ListTag() {
        for (auto it = list.begin(); it != list.end(); ++it) {
            delete *it;
        }
        list.clear();
    }

    Tag* copy() override {
        ListTag<T>* res = new ListTag<T>(getName());
        res->type = type;
        for (auto it = list.begin(); it != list.end(); ++it) {
            res->list.push_back((*it)->copy());
        }
        return res;
    }

    bool equals(Tag* obj) override {
        if (Tag::equals(obj)) {
            ListTag<T>* o = dynamic_cast<ListTag<T>*>(obj);
            if (o && type == o->type) {
                if (list.size() != o->list.size()) return false;

                for (size_t i = 0; i < list.size(); ++i) {
                    if (!list[i]->equals(o->list[i])) {
                        return false;
                    }
                }
                return true;
            }
        }
        return false;
    }
};