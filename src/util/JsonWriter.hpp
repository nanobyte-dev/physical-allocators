#pragma once
#include <iostream>
#include <string>

class JsonWriter
{
public:
    JsonWriter(std::ostream& out, bool pretty);

    void BeginObject();
    void BeginObject(std::string name);
    void EndObject();
    
    void BeginArray();
    void BeginArray(std::string name);
    void EndArray();

    template<typename T>
    void Property(std::string name, T value);

private:
    void Indent();
    void HandleComma();
    std::string Escape(std::string str);

    std::ostream& m_Out;
    bool m_NeedComma;
    bool m_Pretty;
    int m_Lvl;
};

template<typename T>
void JsonWriter::Property(std::string name, T value)
{
    HandleComma();
    m_Out << "\"" << Escape(name) << "\" : ";

    if (std::is_pointer<T>::value)
    {
        if (reinterpret_cast<void*>(value) == nullptr)
            m_Out << "null";
        else m_Out << reinterpret_cast<uint64_t>(value);
    }
    else
    {
        m_Out << value;
    }

    m_NeedComma = true;
}

template<>
void JsonWriter::Property<const char*>(std::string name, const char* value);

template<>
void JsonWriter::Property<std::string>(std::string name, std::string value);

template<>
void JsonWriter::Property<bool>(std::string name, bool value);