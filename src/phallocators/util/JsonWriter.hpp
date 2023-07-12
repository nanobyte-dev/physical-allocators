#pragma once
#include <iostream>
#include <string>

class JsonWriter
{
public:
    JsonWriter(std::ostream& out, bool pretty);

    void BeginObject();
    void BeginObject(const std::string& name);
    void EndObject();
    
    void BeginArray();
    void BeginArray(const std::string& name);
    void EndArray();

    template<typename T>
    void Property(const std::string& name, T value);

private:
    void Indent();
    void HandleComma();
    static std::string Escape(const std::string& str);

    std::ostream& m_Out;
    bool m_NeedComma;
    bool m_Pretty;
    int m_Lvl;
};

template<typename T>
void JsonWriter::Property(const std::string& name, T value)
{
    HandleComma();
    m_Out << "\"" << Escape(name) << "\" : ";

    if (std::is_pointer<T>::value)
    {
        if (reinterpret_cast<void*>(value) == nullptr)
            m_Out << "null";
        else m_Out << (uint64_t)(value);
    }
    else
    {
        m_Out << value;
    }

    m_NeedComma = true;
}

template<>
void JsonWriter::Property<const char*>(const std::string& name, const char* value);

template<>
void JsonWriter::Property<std::string>(const std::string& name, std::string value);

template<>
void JsonWriter::Property<bool>(const std::string& name, bool value);