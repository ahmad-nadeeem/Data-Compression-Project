# BZip2 Compression Stage 1 - Burrows-Wheeler Transform & RLE

This project implements the first phase of a BZip2-style compression pipeline. It includes file block management, Run-Length Encoding (RLE-1), and the Burrows-Wheeler Transform (BWT) to prepare data for higher efficiency compression.

---

## 🚀 Features

- **Block Management** Automatically divides large input files into manageable blocks based on a configurable size.

- **RLE-1 Encoding/Decoding** A primary run-length encoding stage that collapses repeating byte sequences into `[char, count]` pairs.

- **Burrows-Wheeler Transform (BWT)** Rearranges data into runs of similar characters to improve compression ratios using a rotation matrix approach.

- **Configurable Settings** Uses an external `config.ini` file to toggle features and set block sizes.

- **Trace System** Provides a detailed step-by-step debug view of the data as it moves through each transformation stage.

---

## 📂 Project Structure

| File        | Description |
|------------|------------|
| `main.cpp` | Orchestrates the compression trace and block management |
| `bwt.cpp`  | Implements the Burrows-Wheeler Transform (Encode/Decode) |
| `rle.cpp`  | Implements the Stage 1 Run-Length Encoding logic |
| `config.cpp` | Handles parsing of the `config.ini` settings |
| `bzip2.h`  | Global headers, data structures, and function prototypes |
| `Makefile` | Build instructions for Linux and cross-compilation for Windows |

---

## 🛠️ Getting Started

### 📋 Prerequisites
- GCC/G++ Compiler  
- Make  

---

### ⚙️ Compilation & Execution

#### 🐧 Linux
Compile the project:
`make`

Run the executable:
`./bzip2_impl`

#### 🪟 Windows (Cross-Compilation via MinGW)
Compile the project:
`make windows`

Run the executable:
`bzip2_impl.exe`

---

## ⚙️ Configuration

Modify `config.ini` to adjust the compression parameters:

- `block_size` → Maximum size of each data block in bytes  
- `rle1_enabled` → Toggle the initial RLE pass  
- `bwt_type` → Method used for BWT (default: `matrix`)  

---

## 🔄 Transformation Workflow

### 1. Original Input
Data is read from `test.txt` and divided into blocks.

### 2. RLE-1 Encode
Consecutive identical characters are compressed.  
**Example:** `BBBB → [B, 4]`

### 3. BWT Encode
- Data is lexicographically rotated  
- Last column of the rotation matrix is stored  
- `primary_index` is saved for reconstruction  

### 4. BWT Decode
Reverses the permutation using the next vector transformation.

### 5. RLE-1 Decode
Expands compressed data back to original form.

---

## 🧪 Debug Output Example

When running `bzip2_impl`, the system provides a hex-trace of the transformation:

=== 1. ORIGINAL INPUT (Size: 18) ===
Data [Char|Hex]: [A|41] [B|42] [B|42] ...

=== 2. AFTER RLE-1 ENCODE (Size: 10) ===
Data [Char|Hex]: [A|41] [.|01] [B|42] ...

=== 3. AFTER BWT ENCODE (Size: 10) ===
Primary Index: 2
Data [Char|Hex]: [D|44] [D|44] [A|41] ...

=========================================
SUCCESS! Decoded data matches original block perfectly.

---

## 👥 Project Team

- **Ahmad Nadeem** (22I-1162) — [Add brief contribution here]  
- **Iman Qamar** (22L-6854) — [Add brief contribution here]  
- **Noor Ul Amin** (22L-6675) — [Add brief contribution here]  

**Course Instructor:** Dr. Faisal Aslam
