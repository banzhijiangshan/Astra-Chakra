import matplotlib.pyplot as plt
import numpy as np

x = [8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576, 2097152, 4194304, 8388608, 16777216, 33554432, 67108864, 134217728, 268435456, 536870912, 1073741824, 2147483648]
y = [240.352, 240.352, 240.376, 240.376, 240.376, 240.376, 240.400, 240.460, 240.568, 240.796, 241.216, 242.080, 243.808, 247.300, 254.284, 268.252, 296.164, 352.000, 463.684, 687.016, 1133.716, 2027.092, 3813.916, 7387.456, 14534.536, 28828.720, 57417.088, 114593.836, 228947.334]

x_even = np.linspace(0, len(x) - 1, len(x))
plt.plot(x_even, y, marker='.')

plt.xticks(x_even, x, rotation=45, fontsize=8)

plt.title('4 GPUs All-Reduce')
plt.xlabel('comm size(bytes)')
plt.ylabel('time(us)')
plt.subplots_adjust(left=0.15, right=0.9, top=0.9, bottom=0.2)

plt.savefig('line_plot.png')

plt.show()
