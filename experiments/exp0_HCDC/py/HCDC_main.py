import numpy as np
import HCDC_encoder as enc_hamm
import HCDC_decoder as dec_hamm
from pathlib import Path
import sys


class Tee:
    def __init__(self, *files):
        self.files = files

    def write(self, text):
        for file in self.files:
            file.write(text)
            file.flush()

    def flush(self):
        for file in self.files:
            file.flush()


log_file = open("HCDC.log", "w", encoding="utf-8")
sys.stdout = Tee(sys.stdout, log_file)

file_names = [
    "bib",
    "book1",
    "book2",
    "paper1",
    "paper2",
    "paper3",
    "paper4",
    "paper5",
    "paper6"
]
large_file_names = [
    "bible.txt",
    "E.coli",
    "world192.txt"
]

output_file_names = file_names# + large_file_names

output_dir = Path("dec_enc_data")
output_dir.mkdir(parents=True, exist_ok=True)


for file_name in output_file_names:

    print()
    print("=" * 60)
    print("File:", file_name)
    print("=" * 60)

    input_path = Path("data") / file_name

    decoded_path = output_dir / (
        f"{input_path.stem}_decoded{input_path.suffix}"
    )


    # --------------------------------------------------
    # Read file
    # --------------------------------------------------

    with open(input_path, "rb") as f:
        data = f.read()


    unique_values = set(data)

    # --------------------------------------------------
    # Check if direct 7-bit representation is possible
    # --------------------------------------------------

    if max(unique_values) >= 128:
        print("SKIPPED: The file contains values >= 128")
        print("The current direct 7-bit representation is not suitable.")
        continue


    # --------------------------------------------------
    # byte -> 7 bits
    # --------------------------------------------------

    data_array = []

    for byte in data:
        bits = f"{byte:07b}"
        data_array.extend(map(int, bits))

    print("Original length:", len(data_array), "bits")

    # --------------------------------------------------
    # HCDC encode
    # --------------------------------------------------

    encoded_data, num_iterations = enc_hamm.encode_n_times(data_array)

    print("Encoded length:", len(encoded_data))
    print("Number of iterations:", num_iterations)
    compression_percentage = (len(data_array) - len(encoded_data)) / len(data_array) * 100
    print(f"Compression percentage: {compression_percentage:.1f}%")

    # --------------------------------------------------
    # HCDC decode
    # --------------------------------------------------

    decoded_data = dec_hamm.decode_n_times(encoded_data)

    if len(decoded_data) % 7 != 0:
        raise ValueError(
            f"{file_name}: decoded data length is not divisible by 7"
        )

    # --------------------------------------------------
    # 7 bits -> byte
    # --------------------------------------------------

    byte_values = []

    for j in range(0, len(decoded_data), 7):

        symbol_bits = decoded_data[j:j+7]

        value = int(
            ''.join(map(str, symbol_bits)),
            2
        )

        byte_values.append(value)

    decoded_bytes = bytes(byte_values)

    # --------------------------------------------------
    # Write decoded file
    # --------------------------------------------------

    with open(decoded_path, "wb") as f:
        f.write(decoded_bytes)

    # --------------------------------------------------
    # Verification
    # --------------------------------------------------

    if data == decoded_bytes:
        print("FILES ARE IDENTICAL")
    else:
        print("FILES ARE DIFFERENT")
