from mcap_protobuf.decoder import DecoderFactory

from mcap.reader import make_reader

mcap_file_path = "/Users/huey/Downloads/example-023-av-argo-2.mcap"


def main():
    with open(mcap_file_path, "rb") as f:
        reader = make_reader(f, decoder_factories=[DecoderFactory()])
        for schema, channel, message, proto_msg in reader.iter_decoded_messages():
            if channel.topic == "/markers/car":
                print(f"{channel.topic} {schema.name} [{message.log_time}]: {proto_msg}")


if __name__ == "__main__":
    main()
