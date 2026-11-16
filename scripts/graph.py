import matplotlib.pyplot as plt
import pandas as pd

# Load the external file
# Replace 'your_file.csv' with the actual path to your file
df = pd.read_csv('./bench_kdyna_4coeurs.csv')

# IMPORTANT: Always sort by PID before plotting to avoid zig-zag lines
df = df.sort_values(by='pid')

# Now you can use df['pid'], df['avg_delay_ms'], etc. in your plotting code

# 2. Initialize the plot
fig, ax1 = plt.subplots(figsize=(12, 6))

# Plotting Average Delay on the primary Y-axis (Left)
color_avg = 'tab:blue'
ax1.set_xlabel('PID', fontsize=12)
ax1.set_ylabel('Average Delay (ms)', color=color_avg, fontsize=12, fontweight='bold')
line1 = ax1.plot(df['pid'], df['avg_delay_ms'], color=color_avg, marker='o', label='Avg Delay', linewidth=2)
ax1.tick_params(axis='y', labelcolor=color_avg)

# Create a secondary Y-axis for the Maximum Delay (Right)
ax2 = ax1.twinx()
color_max = 'tab:red'
ax2.set_ylabel('Maximum Delay (ms)', color=color_max, fontsize=12, fontweight='bold')
line2 = ax2.plot(df['pid'], df['max_delay_ms'], color=color_max, marker='x', linestyle='--', label='Max Delay', linewidth=2)
ax2.tick_params(axis='y', labelcolor=color_max)

# 3. Add Titles and Grid
plt.title('Performance scx_dyna avec 4 cœurs', fontsize=14, pad=20)
ax1.grid(True, which='both', linestyle='--', alpha=0.5)

ax1.set_xticks(df['pid'])

# Combine legends from both axes into one box
lines = line1 + line2
labels = [l.get_label() for l in lines]
ax1.legend(lines, labels, loc='upper left')

# 4. Final adjustments and saving
plt.tight_layout()
# plt.savefig('my_performance_chart.png', dpi=300) # Uncomment to save the file
plt.show()
