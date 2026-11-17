import matplotlib.pyplot as plt
import pandas as pd

# 1. Load both external files
df1 = pd.read_csv('./benchmark_values/bench.csv')
df2 = pd.read_csv('./benchmark_values/bench2.csv')  # Update with your second file path

# Sort by PID and reset index so both align cleanly on 0, 1, 2...
df1 = df1.sort_values(by='pid').reset_index(drop=True)
df2 = df2.sort_values(by='pid').reset_index(drop=True)

# Create a shared numerical x-axis and custom labels ("Pid 1", "Pid 2", ...)
x_indices = range(len(df1))
pid_labels = [f'Pid {i + 1}' for i in x_indices]

# 2. Initialize the plot
fig, ax1 = plt.subplots(figsize=(12, 6))

# --- Primary Y-axis (Left): Average Delay ---
ax1.set_xlabel('Process', fontsize=12)
ax1.set_ylabel('Average Delay (ms)', color='tab:blue', fontsize=12, fontweight='bold')

line1 = ax1.plot(x_indices, df1['avg_delay_ms'], color='tab:blue', marker='o',
                 label='Avg Delay (Run 1)', linewidth=2)
line3 = ax1.plot(x_indices, df2['avg_delay_ms'], color='tab:orange', marker='o',
                 label='Avg Delay (Run 2)', linewidth=2)

ax1.tick_params(axis='y', labelcolor='tab:blue')

# --- Secondary Y-axis (Right): Maximum Delay ---
ax2 = ax1.twinx()
ax2.set_ylabel('Maximum Delay (ms)', color='tab:purple', fontsize=12, fontweight='bold')

line2 = ax2.plot(x_indices, df1['max_delay_ms'], color='tab:purple', marker='x',
                 linestyle='--', label='Max Delay (Run 1)', linewidth=2)
line4 = ax2.plot(x_indices, df2['max_delay_ms'], color='tab:red', marker='x',
                 linestyle='--', label='Max Delay (Run 2)', linewidth=2)

ax2.tick_params(axis='y', labelcolor='tab:purple')

# 3. Configure X-axis labels, Titles, and Grid
ax1.set_xticks(x_indices)
ax1.set_xticklabels(pid_labels, rotation=45, ha='right')  # Rotation prevents label overlap

plt.title('EEVDF vs ksched', fontsize=14, pad=20)
ax1.grid(True, which='both', linestyle='--', alpha=0.5)

# Combine legends from both axes into one box
lines = line1 + line3 + line2 + line4
labels = [l.get_label() for l in lines]
ax1.legend(lines, labels, loc='upper left')

# 4. Final adjustments and saving
plt.tight_layout()
# plt.savefig('my_performance_chart.png', dpi=300)
plt.show()
