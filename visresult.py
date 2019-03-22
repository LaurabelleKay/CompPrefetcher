import json
import matplotlib.pyplot as plt
import numpy as np

def main():
    """main"""
    with open('format.json') as results:
        data = json.load(results)

    b_num = len(data["Benchmark"])

    for i in range(0, b_num):
        benchmark = data["Benchmark"][i]["Name"]
        print(benchmark)
        p_num = len(data["Benchmark"][i]["Prefetcher"])

        for j in range(0, p_num):
            prefetcher = data["Benchmark"][i]["Prefetcher"][j]["Name"]
            print(prefetcher)

main()
