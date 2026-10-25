import numpy as np
import random

H = np.array([
    [0, 1, 1, 1, 1, 0, 0],
    [1, 0, 1, 1, 0, 1, 0],
    [1, 1, 0, 1, 0, 0, 1]
], dtype=np.uint8)

G = np.array([
    [1, 0, 0, 0, 0, 1, 1],
    [0, 1, 0, 0, 1, 0, 1],
    [0, 0, 1, 0, 1, 1, 0],
    [0, 0, 0, 1, 1, 1, 1]
], dtype=np.uint8)

hamming_values = np.array([
    0, 15, 22, 25,
    37, 42, 51, 60,
    67, 76, 85, 90,
    102, 105, 112, 127
])

def bits_to_int(bits):
    return int(''.join(map(str, bits)), 2)

def parse_one_header(encoded_signal, pos):
    tail_len = bits_to_int(
        encoded_signal[pos:pos+3]
    )
    pos += 3
    tail = encoded_signal[pos:pos+tail_len]
    pos += tail_len
    num_pairs = bits_to_int(
        encoded_signal[pos:pos+5]
    )
    pos += 5
    counts_from_header = []
    hamming_from_header = []
    for i in range(num_pairs):
        original_value = bits_to_int(
            encoded_signal[pos:pos+7]
        )
        pos += 7
        hamming_value = bits_to_int(
            encoded_signal[pos:pos+7]
        )
        pos += 7
        counts_from_header.append(original_value)
        hamming_from_header.append(hamming_value)
    header = (
        np.array(tail, dtype=np.uint8),
        counts_from_header,
        hamming_from_header
    )
    return header, pos

def parse_encoded_signal(encoded_signal):
    N = bits_to_int(encoded_signal[:8])
    pos = 8
    headers = []
    for i in range(N):
        header, pos = parse_one_header(
            encoded_signal,
            pos
        )
        headers.append(header)
    encoded_data = encoded_signal[pos:]
    return N, headers, encoded_data
def dec_hcdc(enc_data):
  decoder = []
  pos = 0
  while pos < len(enc_data):
    if enc_data[pos] == 0:
      decoder.extend(enc_data[pos + 1 : pos + 8])
      pos = pos + 8
    else:
      decoder.extend( (enc_data[pos + 1 : pos + 5] @ G) % 2)
      pos = pos + 5
  return np.array(decoder, dtype=np.uint8)

def reverse_precoding(data, counts_from_header, hamming_from_header):
    decoded_data = data.copy()
    n_blocks = len(data) // 7
    for i in range(n_blocks):
        block = data[i*7:(i+1)*7]
        value = int(''.join(map(str, block)), 2)
        for j in range(len(hamming_from_header)):
            if value == hamming_from_header[j]:
                new_value = counts_from_header[j]
                new_bits = np.array(
                    list(map(int, f"{new_value:07b}")),
                    dtype=np.uint8
                )
                decoded_data[i*7:(i+1)*7] = new_bits
                break
            elif value == counts_from_header[j]:
                new_value = hamming_from_header[j]
                new_bits = np.array(
                    list(map(int, f"{new_value:07b}")),
                    dtype=np.uint8
                )
                decoded_data[i*7:(i+1)*7] = new_bits
                break
    return decoded_data

def full_decode(encoded_data, header):
    tail, counts_from_header, hamming_from_header = header
    decoded_data = dec_hcdc(encoded_data)
    decoded_data = reverse_precoding(
        decoded_data,
        counts_from_header,
        hamming_from_header
    )
    original_data = np.concatenate([
        decoded_data,
        tail
    ])
    return original_data

def decode_n_times(data):
    N, headers, encoded_data = parse_encoded_signal(data)
    for i in range(N - 1, -1, -1):
        encoded_data = full_decode(
            encoded_data,
            headers[i]
        )
    return encoded_data