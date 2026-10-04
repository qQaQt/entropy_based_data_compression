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

def syndrome(x):
    x = np.array(x, dtype=np.uint8)
    return (x @ H.T) % 2
x = np.array([1, 0, 0, 0, 1, 0, 1])

def int_to_bits(value, width):
    return np.array(
        list(map(int, f"{value:0{width}b}")),
        dtype=np.uint8
    )

def hcdc(x):
    s = syndrome(x)
    if np.array_equal(s, np.zeros(3)):
      x_out = x[:4]
      x_out = np.append(1, x[:4])
    else:
      x_out = x
      x_out = np.append(0, x)
    return(x_out)

def enc_hcdc(data):
    result = []
    for i in range(n):
       result.append(hcdc(data[i*7:(i+1)*7]))
    return(np.concatenate(result))

def precoding(data):
  precoding_array = data.copy()
  for i in range(n):
      block = data[i*7:(i+1)*7]
      value = int(''.join(map(str, block)), 2)

      for j in range(len(hamming_values_fltrd)):
          if value == counts_max16_fltrd[j]:
              new_value = hamming_values_fltrd[j]
              new_bits = np.array(list(map(int, f"{new_value:07b}")))
              precoding_array[i*7:(i+1)*7] = new_bits
              break

          elif value == hamming_values_fltrd[j]:
              new_value = counts_max16_fltrd[j]
              new_bits = np.array(list(map(int, f"{new_value:07b}")))
              precoding_array[i*7:(i+1)*7] = new_bits
              break
  return(precoding_array)

def create_header(tail, counts_max16_fltrd, hamming_values_fltrd):

    header = []

    header.extend(int_to_bits(len(tail), 3))

    header.extend(tail)

    num_pairs = len(counts_max16_fltrd)
    header.extend(int_to_bits(num_pairs, 5))

    for i in range(num_pairs):

        original_value = counts_max16_fltrd[i]
        hamming_value = hamming_values_fltrd[i]

        header.extend(int_to_bits(original_value, 7))

        header.extend(int_to_bits(hamming_value, 7))

    return np.array(header, dtype=np.uint8)

def full_encode(data):
    global n
    global counts_max16_fltrd
    global hamming_values_fltrd
    n = len(data) // 7
    main_data = data[:n * 7]
    tail = data[n * 7:]
    counts = np.zeros(128, dtype=int)
    for i in range(n):
        block = main_data[i*7:(i+1)*7]
        value = int(''.join(map(str, block)), 2)
        counts[value] += 1
    counts_max16 = np.argsort(counts)[-16:]
    common = set(counts_max16) & set(hamming_values)
    counts_max16_fltrd = [
        x for x in counts_max16
        if x not in common
    ]
    hamming_values_fltrd = [
        x for x in hamming_values
        if x not in common
    ]
    precoding_data = precoding(main_data)
    encoded_data = enc_hcdc(precoding_data)
    header = create_header(
        tail,
        counts_max16_fltrd,
        hamming_values_fltrd
    )
    return header, encoded_data

def encode_n_times(data):
    encoded_data = np.asarray(data, dtype=np.uint8)
    headers = []
    iteration = 0
    while iteration < 8:
        current_len = len(encoded_data)
        new_header, new_encoded_data = full_encode(encoded_data)
        new_len = len(new_encoded_data)
        #if new_len >= current_len:
        #    break
        encoded_data = new_encoded_data
        headers.append(new_header)
        iteration += 1
    N = iteration
    iterations_bits = int_to_bits(N, 8)
    if len(headers) > 0:
        all_headers = np.concatenate(headers)
    else:
        all_headers = np.array([], dtype=np.uint8)
    result = np.concatenate([
        iterations_bits,
        all_headers,
        encoded_data,
    ])
    return result, iteration