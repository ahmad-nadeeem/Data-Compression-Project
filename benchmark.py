#!/usr/bin/env python3
import os
import time
import subprocess
import pandas as pd
import matplotlib.pyplot as plt

# =============================================================================
# CONFIGURATION
# =============================================================================
EXECUTABLE = "./bzip2_test"             # Name of your compiled C++ program
BENCHMARK_DIR = "./benchmarks/"         # Where your test text files live
RESULTS_DIR = "./results/"              # Where everything gets saved
RESULTS_FILE = os.path.join(RESULTS_DIR, "results.csv")
BLOCK_SIZE = 500000

def run_benchmarks():
    print("Starting Automated Benchmarking with REAL Output Files & Logging...\n")
    
    # Create required directories safely
    os.makedirs(RESULTS_DIR, exist_ok=True)
    os.makedirs(os.path.join(RESULTS_DIR, "compressed_files"), exist_ok=True)
    os.makedirs(os.path.join(RESULTS_DIR, "logs"), exist_ok=True)
    
    # Initialize the CSV file with headers
    with open(RESULTS_FILE, "w") as f:
        f.write("File,Size,BlockSize,CompressionRatio,Time,Memory\n")

    results = []
    
    # Loop through every .txt file in the benchmark folder
    for filename in os.listdir(BENCHMARK_DIR):
        if not filename.endswith(".txt"): 
            continue
            
        filepath = os.path.join(BENCHMARK_DIR, filename)
        if os.path.getsize(filepath) == 0: 
            print(f"Skipping empty file: {filename}")
            continue
        
        # Where to save the final .bz2 file
        output_filepath = os.path.join(RESULTS_DIR, "compressed_files", filename + ".bz2")
        # Where to save the step-by-step text log
        log_filepath = os.path.join(RESULTS_DIR, "logs", filename + "_pipeline.log")
        
        start_time = time.time()
        
        # Execute C++ program: ./bzip2_test <input> <output> --verbose
        try:
            process = subprocess.run(
                [EXECUTABLE, filepath, output_filepath, "--verbose"], 
                capture_output=True, 
                text=True,
                check=True # Raise an error if C++ crashes
            )
        except subprocess.CalledProcessError as e:
            print(f"[ERROR] C++ Program Crashed on {filename}!")
            print(f"Details saved to {log_filepath}")
            with open(log_filepath, "w") as log_file:
                log_file.write("=== CRASH LOG ===\n")
                log_file.write(e.stderr if e.stderr else e.stdout)
            continue
            
        execution_time = time.time() - start_time
        output_text = process.stdout
        
        # Save the massive step-by-step pipeline output to a text file
        with open(log_filepath, "w") as log_file:
            log_file.write(output_text)
            if process.stderr:
                log_file.write("\nERRORS:\n" + process.stderr)

        # Extract just the METRICS line for our CSV
        output_lines = output_text.strip().split('\n')
        metrics_line = [line for line in output_lines if line.startswith("METRICS:")]
        
        if metrics_line:
            # Parse: METRICS:original_size,compressed_size,memory_used
            data = metrics_line[0].replace("METRICS:", "").split(",")
            original_size = float(data[0])
            compressed_size = float(data[1])
            memory_used = float(data[2])
            
            compression_ratio = original_size / compressed_size if compressed_size > 0 else 1.0
            
            # Save to list
            results.append(f"{filename},{original_size},{BLOCK_SIZE},{compression_ratio:.2f},{execution_time:.4f},{memory_used:.2f}")
            
            # Print success to terminal
            print(f"Processed: {filename}")
            print(f"  ├─ Ratio: {compression_ratio:.2f} | Mem: {memory_used:.2f}MB | Time: {execution_time:.4f}s")
            
            # Check if the physical file was actually created
            if os.path.exists(output_filepath):
                actual_file_size = os.path.getsize(output_filepath)
                print(f"  ├─ Output File saved: {output_filepath} ({actual_file_size} bytes)")
            
            print(f"  └─ Pipeline Steps saved: {log_filepath}\n")
        else:
            print(f"[ERROR] Failed to get metrics for {filename}. Check {log_filepath} for details.\n")

    # Write all results to the CSV
    with open(RESULTS_FILE, "a") as f:
        for line in results:
            f.write(line + "\n")
            
    print(f"Benchmarking complete. Results saved to {RESULTS_FILE}\n")

def generate_graphs():
    print("Generating Performance Graphs...")
    if not os.path.exists(RESULTS_FILE): 
        print("No results file found. Run benchmarks first.")
        return
        
    df = pd.read_csv(RESULTS_FILE)
    if df.empty: 
        print("Results file is empty.")
        return

    # Graph 1: Time
    plt.figure(figsize=(10, 6))
    plt.bar(df['File'], df['Time'], color='skyblue')
    plt.title('Compression Time by File')
    plt.xlabel('File')
    plt.ylabel('Time (Seconds)')
    plt.xticks(rotation=45, ha='right') # Rotate labels so they don't overlap
    plt.tight_layout()
    plt.savefig(os.path.join(RESULTS_DIR, "time_graph.png")) 
    
    # Graph 2: Compression Ratio
    plt.figure(figsize=(10, 6))
    plt.bar(df['File'], df['CompressionRatio'], color='lightgreen')
    plt.title('Compression Ratio by File')
    plt.xlabel('File')
    plt.ylabel('Ratio (Original Size / Compressed Size)')
    plt.xticks(rotation=45, ha='right')
    plt.tight_layout()
    plt.savefig(os.path.join(RESULTS_DIR, "ratio_graph.png")) 
    
    print(f"Graphs saved to {RESULTS_DIR} directory.")

if __name__ == "__main__":
    run_benchmarks()
    generate_graphs()
