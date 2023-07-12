#include "JsonWriter.hpp"

JsonWriter::JsonWriter(std::ostream& out, bool pretty)
    : m_Out(out),
      m_NeedComma(false),
      m_Pretty(pretty),
      m_Lvl(0)
{
}

void JsonWriter::BeginObject()
{
    HandleComma();
    
    m_Out << "{";

    m_NeedComma = false;
    m_Lvl++;

    if (m_Pretty) {
        m_Out << "\n";
        Indent();
    }
}

void JsonWriter::BeginObject(const std::string& name)
{
    HandleComma();

    m_Out << "\"" << Escape(name) << "\" : {";
    m_NeedComma = false;
    m_Lvl++;

    if (m_Pretty) {
        m_Out << "\n";
        Indent();
    }
}

void JsonWriter::EndObject()
{
    m_Lvl--;

    if (m_Pretty) {
        m_Out << "\n";
        Indent();
    }

    m_Out << "}";
    m_NeedComma = true;
}

void JsonWriter::BeginArray()
{
    m_Out << "[";
    m_NeedComma = false;
    m_Lvl++;

    if (m_Pretty) {
        m_Out << "\n";
        Indent();
    }
}

void JsonWriter::BeginArray(const std::string& name)
{
    HandleComma();

    m_Out << "\"" << Escape(name) << "\" : [";
    m_NeedComma = false;
    m_Lvl++;

    if (m_Pretty) {
        m_Out << "\n";
        Indent();
    }
}

void JsonWriter::EndArray()
{
    m_Lvl--;

    if (m_Pretty) {
        m_Out << "\n";
        Indent();
    }

    m_Out << "]";
    m_NeedComma = true;
}

void JsonWriter::Indent()
{
    for (int i = 0; i < m_Lvl; i++)
        m_Out << "  ";
}

void JsonWriter::HandleComma()
{
    if (m_NeedComma) 
    {
        m_Out << ",";
        if (m_Pretty) {
            m_Out << "\n";
            Indent();
        }
    }
}

std::string JsonWriter::Escape(const std::string& str)
{
	std::string escapedStr = str;
    auto pos = escapedStr.find('\"');
    while (pos != std::string::npos)
    {
	    escapedStr.insert(escapedStr.begin() + pos, '\\');
        pos = escapedStr.find('\"', pos + 2);
    }

    return str;
}

template<>
void JsonWriter::Property<const char*>(const std::string& name, const char* value)
{
    HandleComma();
    m_Out << "\"" << Escape(name) << "\" : \"" << Escape(value) << "\"";
    m_NeedComma = true;
}

template<>
void JsonWriter::Property<std::string>(const std::string& name, std::string value)
{
    HandleComma();
    m_Out << "\"" << Escape(name) << "\" : \"" << Escape(value) << "\"";
    m_NeedComma = true;
}

template<>
void JsonWriter::Property<bool>(const std::string& name, bool value)
{
    HandleComma();
    m_Out << "\"" << Escape(name) << "\" : " << (value ? "true" : "false");
    m_NeedComma = true;
}