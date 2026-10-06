# HCDC Compression Results

The table below compares the compression ratios obtained with the current HCDC implementation with the results reported in the referenced work.

The compression ratio is calculated as `C = original file size / compressed size`, where the original file size is measured using 8 bits per symbol. A larger value of `C` means better compression. `k` is the number of HCDC iterations.

| File | Published HCDC(1) | Published HCDC(k) | My best C | Published k | My k |
| ---    | ---:  | ---:  | ---:  | ---: | ---: |
| bib    | 1.345 | 1.632 | 1.345 | 4 | 1 |
| book1  | 1.450 | 2.543 | 1.469 | 6 | 2 |
| book2  | 1.425 | 2.253 | 1.425 | 5 | 1 |
| paper1 | 1.385 | 1.895 | 1.384 | 4 | 1 |
| paper2 | 1.446 | 2.489 | 1.448 | 6 | 2 |
| paper3 | 1.441 | 2.414 | 1.440 | 5 | 1 |
| paper4 | 1.437 | 2.421 | 1.433 | 6 | 2 |
| paper5 | 1.392 | 1.985 | 1.388 | 5 | 1 |
| paper6 | 1.375 | 1.825 | 1.374 | 4 | 1 |