import pymupdf4llm
markdown = pymupdf4llm.to_markdown("/home/matteo.ielacqua/repos/DistributedKnapsack/doc/1-s2.0-S0167819115000113-main.pdf")

with open("/home/matteo.ielacqua/repos/DistributedKnapsack/doc/1-s2.0-S0167819115000113-main.txt", "w") as f:
    f.write(markdown)
    pass