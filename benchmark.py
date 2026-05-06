#!/usr/bin/env python3
import os
import time
import subprocess
import pandas as pd
import matplotlib.pyplot as plt


EXECUTABLE = "./bzip2_impl"             
BENCHMARK_DIR = "./benchmarks/"         
RESULTS_FILE = "./results/results.csv"  
BLOCK_SIZE = 500000

def run_benchmarks():
    print("Starting Automated Benchmarking with REAL Metrics...")
    
    os.makedirs("./results", exist_ok=True)
    
    with open(RESULTS_FILE, "w") as f:
        f.write("File,Size,BlockSize,CompressionRatio,Time,Memory\n")

    results = []
    
    for filename in os.listdir(BENCHMARK_DIR):
        if not filename.endswith(".txt"): 
            continue
            
        filepath = os.path.join(BENCHMARK_DIR, filename)
        if os.path.getsize(filepath) == 0: continue
        
        start_time = time.time()
        

        process = subprocess.run([EXECUTABLE, filepath], capture_output=True, text=True)
        execution_time = time.time() - start_time
        
        output_lines = process.stdout.strip().split('\n')
        metrics_line = [line for line in output_lines if line.startswith("METRICS:")]
        
        if metrics_line:
            data = metrics_line[0].replace("METRICS:", "").split(",")
            original_size = float(data[0])
            compressed_size = float(data[1])
            memory_used = float(data[2])
            
            compression_ratio = original_size / compressed_size if compressed_size > 0 else 1.0
            
            results.append(f"{filename},{original_size},{BLOCK_SIZE},{compression_ratio:.2f},{execution_time:.4f},{memory_used:.2f}")
            print(f"Processed {filename} -> Ratio: {compression_ratio:.2f} | Mem: {memory_used:.2f}MB | Time: {execution_time:.4f}s")
        else:
            print(f"Failed to get metrics for {filename}. Did it compile properly?")

    with open(RESULTS_FILE, "a") as f:
        for line in results:
            f.write(line + "\n")
            
    print(f"\nBenchmarking complete. Results saved to {RESULTS_FILE}")

def generate_graphs():
    print("Generating Performance Graphs...")
    if not os.path.exists(RESULTS_FILE): return
        
    df = pd.read_csv(RESULTS_FILE)
    if df.empty: return

    plt.figure(figsize=(10, 6))
    plt.bar(df['File'], df['Time'], color='skyblue')
    plt.title('Real Compression Time by File')
    plt.xlabel('File')
    plt.ylabel('Time (Seconds)')
    plt.tight_layout()
    plt.savefig("./results/time_graph.png") 
    
    plt.figure(figsize=(10, 6))
    plt.bar(df['File'], df['CompressionRatio'], color='lightgreen')
    plt.title('Real Compression Ratio by File')
    plt.xlabel('File')
    plt.ylabel('Ratio (Original / Compressed)')
    plt.tight_layout()
    plt.savefig("./results/ratio_graph.png") 
    print("Graphs saved to ./results/ directory.")

if __name__ == "__main__":
    run_benchmarks()
    generate_graphs()
