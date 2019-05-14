import json
import matplotlib.pyplot as plt
import numpy as np
from scipy.stats.mstats import gmean


def column(array, i):
    return [row[i] for row in array]

def main():
    """main"""
    with open('final.json') as results:
        data = json.load(results)

    b_num = len(data["Benchmark"])
    b_name = []
    ipc_array = []
    pf_name = []
    acc_array = []
    geomean = []
    p_num = 0
    for i in range(0, b_num):
        benchmark = data["Benchmark"][i]["Name"]
        b_name.append(benchmark)
        #p_num = 4
        p_num = len(data["Benchmark"][i]["Prefetcher"])
        ac_a = []
        i_a = []
        p_n = []
        for j in range(1, p_num):
            prefetcher = data["Benchmark"][i]["Prefetcher"][j]["Name"]
            p_n.append(prefetcher)
            acc = data["Benchmark"][i]["Prefetcher"][j]["Accuracy"]
            ipc = data["Benchmark"][i]["Prefetcher"][j]["Cumulative IPC"]
            ac_a.append(acc)
            i_a.append(ipc)
        ipc_array.append(i_a)
        pf_name.append(p_n)
        acc_array.append(ac_a)
        #print(pf_name)

    fig, ax = plt.subplots()
    index = np.arange(b_num)
    bar_width = 0.1
    opacity = 1
    colours = ((0.050, 0.658, 0, opacity), (1, 0.039, 0.113, opacity), (0.058, 0.541, 1, opacity), (1, 0.733, 0.078, opacity),
               (0.086, 0.015, 0.862, opacity), (0.941, 0.509, 0, opacity), (0.580, 0.545, 0.239, opacity))
    for i in range(0, p_num - 1):
        ipcs = column(acc_array, i)
        name = column(pf_name, i)
        rects = plt.bar(index + (i * bar_width), ipcs,
                        width=bar_width, color=colours[i], label=name[0], align="center")

    plt.xlabel("Benchmark")
    plt.ylabel("Accuracy")
    plt.xticks(index + (bar_width * 2), b_name)
    plt.legend(loc='upper center', bbox_to_anchor=(0.65, 1),
              ncol=3, fancybox=True, shadow=True)
    plt.tight_layout()
    plt.show()

    fig, ax = plt.subplots()
    index = np.arange(1)

    for i in range(0, p_num - 1):
        ipcs = column(ipc_array, i)
        gm = gmean(ipcs)
        print(gm)
        name = column(pf_name, i)
        plt.bar(i * bar_width, gm, width=bar_width, color=colours[i], label=name[0], align="center")

    #ipcs = column(ipc_array, 0)
    #gm0 = gmean(ipcs)

    #for i in range(1, p_num):
        #ipcs = column(ipc_array, i)
        #gm = gmean(ipcs)
        #val = ((gm/gm0 - 1)) * 100
        #print(val)

    #plt.xlabel("Benchmark")
    plt.ylabel("Accuracy (Geomean)")
    plt.xticks(index + (bar_width * 3), " ")
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.1),
               ncol=3, fancybox=True, shadow=True)
    plt.tight_layout()
    plt.show()

main()
