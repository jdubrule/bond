#include <iostream>
#include "input_file.h"
#include "cmd_arg_reflection.h"
#include <bond/core/cmdargs.h>
#include <bond/stream/stdio_output_stream.h>
#include <bond/protocol/simple_json_writer.h>

using namespace bf;

inline bool IsValidType(bond::BondDataType type)
{
    return type >= bond::BondDataType::BT_BOOL && type <= bond::BondDataType::BT_WSTRING;
}

template <typename Reader>
bool TryProtocol(Reader reader, int confidence = 5)
{
    try
    {
        bond::BondDataType type;
        uint32_t size;
        uint16_t id;

        reader.ReadStructBegin();
        reader.ReadFieldBegin(type, id);

        for (int i = 0; i < confidence; ++i, reader.ReadFieldEnd(), reader.ReadFieldBegin(type, id))
        {
            if (type == bond::BondDataType::BT_STOP)
                break;

            if (type == bond::BondDataType::BT_STOP_BASE)
                continue;

            if (!IsValidType(type))
                return false;

            if (type == bond::BondDataType::BT_SET || type == bond::BondDataType::BT_LIST)
            {
                bond::BondDataType element_type;

                Reader(reader).ReadContainerBegin(size, element_type);

                if (!IsValidType(element_type))
                    return false;
            }

            if (type == bond::BondDataType::BT_MAP)
            {
                std::pair<bond::BondDataType, bond::BondDataType> element_type;

                Reader(reader).ReadContainerBegin(size, element_type);

                if (!IsValidType(element_type.first) || !IsValidType(element_type.second))
                    return false;
            }

            reader.Skip(type);
        }

        return true;
    }
    catch(const bond::StreamException&)
    {
        return false;
    }
}


Protocol Guess(InputFile input)
{
    uint16_t word;
    bond::CompactBinaryReader<InputFile> cbp(input);
    bond::FastBinaryReader<InputFile>   mbp(input);
    bond::CompactBinaryReader<InputFile> cbp2(input, bond::v2);

    input.Read(word);

    if (word == static_cast<uint16_t>(bond::ProtocolType::FAST_PROTOCOL)
     || word == static_cast<uint16_t>(bond::ProtocolType::COMPACT_PROTOCOL)
     || word == static_cast<uint16_t>(bond::ProtocolType::SIMPLE_PROTOCOL))
        return Protocol::marshal;

    if (TryProtocol(mbp))
        return Protocol::fast;

    if (TryProtocol(cbp))
        return Protocol::compact;

    if (TryProtocol(cbp2))
        return Protocol::compact2;

    return Protocol::simple;
}


struct UnknownSchema;

bond::SchemaDef LoadSchema(const std::string& file)
{
    InputFile input(file), tryJson(input);

    char c;
    tryJson.Read(c);

    return (c == '{')
        ? bond::Deserialize<bond::SchemaDef>(bond::SimpleJsonReader<InputFile>(input))
        : bond::Unmarshal<bond::SchemaDef>(input);
}

template <typename Reader, typename Writer>
void TranscodeFromTo(Reader& reader, Writer& writer, const Options& options)
{
    if (!options.schema.empty() && !options.schema.front().empty())
    {
        bond::SchemaDef schema(LoadSchema(options.schema.front()));
        bond::bonded<void, typename bond::ProtocolReader<typename Reader::Buffer> >(reader, bond::RuntimeSchema(schema)).Serialize(writer);
    }
    else
    {
        bond::bonded<UnknownSchema, typename bond::ProtocolReader<typename Reader::Buffer> >(reader).Serialize(writer);
    }
}


template <typename Writer>
void TranscodeFromTo(InputFile& input, Writer& writer, const Options& options)
{
    if (!options.schema.empty() && !options.schema.front().empty())
    {
        bond::SchemaDef schema(LoadSchema(options.schema.front()));
        bond::SelectProtocolAndApply(bond::RuntimeSchema(schema), input, SerializeTo(writer));
    }
    else
    {
        bond::SelectProtocolAndApply<UnknownSchema>(input, SerializeTo(writer));
    }
}


template <typename Reader>
bool TranscodeFrom(Reader reader, const Options& options)
{
    FILE* file;

    if (options.output == "stdout")
        file = stdout;
    else
        file = fopen(options.output.c_str(), "wb");

    bond::StdioOutputStream out(file);

    switch (options.to)
    {
        case Protocol::compact:
        {
            bond::CompactBinaryWriter<bond::StdioOutputStream> writer(out);
            TranscodeFromTo(reader, writer, options);
            return true;
        }
        case Protocol::compact2:
        {
            bond::CompactBinaryWriter<bond::StdioOutputStream> writer(out, bond::v2);
            TranscodeFromTo(reader, writer, options);
            return true;
        }
        case Protocol::fast:
        {
            bond::FastBinaryWriter<bond::StdioOutputStream> writer(out);
            TranscodeFromTo(reader, writer, options);
            return true;
        }
        case Protocol::simple:
        {
            bond::SimpleBinaryWriter<bond::StdioOutputStream> writer(out);
            TranscodeFromTo(reader, writer, options);
            return true;
        }
        case Protocol::simple2:
        {
            bond::SimpleBinaryWriter<bond::StdioOutputStream> writer(out, bond::v2);
            TranscodeFromTo(reader, writer, options);
            return true;
        }
        case Protocol::json:
        {
            bond::SimpleJsonWriter<bond::StdioOutputStream> writer(out, true, 4, options.all_fields);
            TranscodeFromTo(reader, writer, options);
            return true;
        }
        default:
            return false;
    }
}

template <typename Input>
bool Transcode(Input input, const Options& options)
{
    bf::Protocol from = options.from.empty() ? Protocol::guess : options.from.front();

    if (from == Protocol::guess)
    {
        from = Guess(input);
        std::cerr << std::endl << "Guessed " << ToString(from) << std::endl;
    }

    switch (from)
    {
        case Protocol::marshal:
            return TranscodeFrom(input, options);
        case Protocol::compact:
            return TranscodeFrom(bond::CompactBinaryReader<Input>(input), options);
        case Protocol::compact2:
            return TranscodeFrom(bond::CompactBinaryReader<Input>(input, bond::v2), options);
        case Protocol::fast:
            return TranscodeFrom(bond::FastBinaryReader<Input>(input), options);
        case Protocol::simple:
            return TranscodeFrom(bond::SimpleBinaryReader<Input>(input), options);
        case Protocol::simple2:
            return TranscodeFrom(bond::SimpleBinaryReader<Input>(input, bond::v2), options);
        default:
            return false;
    }
}


int main(int argc, char** argv)
{
    try
    {
        bf::Options options = bond::cmd::GetArgs<bf::Options>(argc, argv);

        if (!options.help)
        {
            InputFile input(options.file);

            do
            {
                // In order to decode multiple payloads from a file we need to
                // use InputFile& however that usage doesn't support marshalled
                // bonded<T> in untagged protocols. As a compromise we use
                // InputFile for the last payload and InputFile& otherwise.
                if (options.schema.size() > 1 || options.from.size() > 1)
                {
                    if (!Transcode<InputFile&>(input, options))
                        return 1;
                }
                else
                {
                    if (!Transcode<InputFile>(input, options))
                        return 1;
                }

                if (!options.schema.empty())
                    options.schema.pop_front();

                if (!options.from.empty())
                    options.from.pop_front();
            }
            while (!options.schema.empty() || !options.from.empty());

            return 0;
        }
    }
    catch(const std::exception& error)
    {
        std::cerr << std::endl << error.what() << std::endl;
    }

    bond::cmd::ShowUsage<bf::Options>(argv[0]);

    return 1;
}

