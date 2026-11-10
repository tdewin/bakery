# bakery
Build your recipe in Markdown, execute it with bakery.

A minimal, zero-dependency C CLI to parse Markdown runbooks, extract bash code blocks by section, inspect or pipe them, and execute steps interactively or unattended.

One shot vibe coded tool, please only use after you have reviewed the code and declared it safe yourself. Use in lab preferably or in a container. If it breaks your environment, it is MIT license, so you are on your own


# how it works

Level 1 header (#) is a step, the first code block marked bash (```bash) is the one that will be utilized. 

````bash
DEMO# cat test.md 
# test 1

```bash
echo hello
```

# test 2

```bash
date
```
````

Running the file to bakery list the two titles
````bash
DEMO# bakery test.md 
  1 : test 1
  2 : test 2
````

Length display the amount of titles
````bash
DEMO# bakery test.md len
2
````

Print the inner code block for 2
````bash
DEMO# bakery test.md print 2
date
````

You could pipe that to sh
````bash
DEMO# bakery test.md print 2 | sh
Tue Nov 10 09:25:03 UTC 2026
````

Or you can execute it directly
````bash
DEMO# bakery test.md exec 2
--- Code to execute (Step 2) ---
date
---------------------------------
Execute this script? [y/N]: y
Tue Nov 10 09:25:12 UTC 2026
````

You can yolo it
````bash
DEMO# bakery test.md exec 2 -y
--- Code to execute (Step 2) ---
date
---------------------------------
Tue Nov 10 09:25:21 UTC 2026
````

Make a nice document to distribute with pandoc. Does require you to map out to /out in your podman command
```
dnf install pandoc -y
pandoc -o /out/test.docx test.md
```

Or make a slide deck
```
pandoc test.md --shift-heading-level-by=1 -o /out/test.pptx
```
