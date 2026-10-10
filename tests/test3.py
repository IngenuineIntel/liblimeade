# the test that is the third (continued)

import os

import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

plt.rcParams["font.family"] = "sans-serif"
plt.rcParams["font.sans-serif"] = ["Helvetica", "Arial", "DejaVu Sans", "Liberation Sans"]

print("Generating graph...")
df = pd.read_csv("test3.csv")
df.columns = df.columns.map(lambda c: "" if pd.isna(c) else str(c).strip())
df = df.loc[:, ~df.columns.map(lambda c: c == "" or c.startswith("Unnamed:"))]
df = df.reset_index(drop=True)

plt.style.use("dark_background")
ax = df.plot(kind="line", alpha=0.65, figsize=(10, 6))
for column, line in zip(df.columns, ax.lines):
	ax.plot(
		df.index,
		df[column].expanding(min_periods=1).mean(),
		color=line.get_color(),
		linestyle="--",
		linewidth=1.5,
		label=f"{column} cuml. avg.",
	)
ax.set_xlabel("iterations")
ax.set_ylabel("time (nanoseconds)")
ax.set_title("Liblimeade Benchmarks")
ax.set_yscale("log")
ax.set_ylim(top=300_000)
ax.yaxis.set_major_formatter(plt.matplotlib.ticker.StrMethodFormatter("{x:,.0f}"))
ax.tick_params(axis="y", which="both", left=True, right=True, labelleft=True, labelright=True)
ax.legend(loc="center left", bbox_to_anchor=(1.12, 0.5))
plt.tight_layout()
plt.savefig("test3.svg", format="svg", dpi=200, bbox_inches="tight")

print("Graph generated!")
os.system("xdg-open test3.svg")
