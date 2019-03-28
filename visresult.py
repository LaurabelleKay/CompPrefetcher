import json
import matplotlib.pyplot as plt
import numpy as np


def column(array, i):
    return [row[i] for row in array]


def main():
    """main"""
    with open('out1.json') as results:
        data = json.load(results)

    b_num = len(data["Benchmark"])
    b_name = []
    ipc_array = []
    pf_name = []
    acc_array = []
    p_num = 0
    for i in range(0, b_num):
        benchmark = data["Benchmark"][i]["Name"]
        b_name.append(benchmark)
        p_num = len(data["Benchmark"][i]["Prefetcher"])
        ac_a = []
        i_a = []
        p_n = []
        for j in range(0, p_num):
            prefetcher = data["Benchmark"][i]["Prefetcher"][j]["Name"]
            p_n.append(prefetcher)
            acc = data["Benchmark"][i]["Prefetcher"][j]["Accuracy"]
            ipc = data["Benchmark"][i]["Prefetcher"][j]["Cumulative IPC"]
            ac_a.append(acc)
            i_a.append(ipc)
        ipc_array.append(i_a)
        pf_name.append(p_n)
        acc_array.append(ac_a)
        print(pf_name)

    fig, ax = plt.subplots()
    index = np.arange(b_num)
    bar_width = 0.35
    opacity = 0.8
    colours = ((0.223, 0.415, 0.694, opacity), (0.243, 0.588, 0.317, opacity), (0.8, 0.145, 0.160, opacity), (0.325, 0.317, 0.329, opacity),
               (0.419, 0.298, 0.603, opacity), (0.572, 0.141, 0.156, opacity), (0.580, 0.545, 0.239, opacity))
    for i in range(0, p_num):
        ipcs = column(ipc_array, i)
        name = column(pf_name, i)
        rects = plt.bar(index + (i * bar_width), ipcs,
                        bar_width, color=colours[i], label=name[0])

    # col_arr0 = column(ipc_array, 0)
    # col_arr1 = column(ipc_array, 1)
    # rects1 = plt.bar(index, col_arr0, bar_width, alpha=opacity, color='b', label="No")
    # rects2 = plt.bar(index + bar_width, col_arr1, bar_width, alpha=opacity, color='g', label="NextN")

    plt.xlabel("Benchmark")
    plt.ylabel("Cumulative IPC")
    plt.xticks(index + bar_width, b_name)
    plt.legend()
    plt.tight_layout()
    plt.show()


main()
