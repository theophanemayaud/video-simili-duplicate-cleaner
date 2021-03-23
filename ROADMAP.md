# Ideas to implement next 
(in approximate order of importance)

- Similar auto delete (keep biggest if) : 

    - same duration

    - different file sizes

    - one has name contained within other file's name

    - similarity = 100%

- Ability to "protect" specific folder, and select specific folders to delete first.

- Look inside photos app library, and tell photos to delete them.

- Somewhow have file name comparison to enable rules based on them, or even just displaying colors based on similarity

- Hint for path : display pathname, like for file name (on hover)

- Exact auto delete (same size, duration etc : which to keep ?) : add name within another option

- Button to clear Database file, as it sometimes gets quite large !!!

## Minor things

- Display all numbers with apostrophe thousand separators to make them more readable.

- Close windows with cmd w

- Clear up variable names for files : paths vs name vs namepath/pathname (eg Video.filename)

- Look into how to avoid errors. Maybe manually review black files, and add open to file location in finder link in the mainwindow error message ?

- Auto trash functions : handle computation times better (noo spinning wheel !)

- Implement a dry run of auto deletes : enables you to know how many would be deleted before really deleting them (but it's not really necessary as it's already moved to trash, so if you want you can just retreive them !)

- Get version number from Plist file and not from added version.txt file, see : https://lucidar.me/en/dev-c-cpp/reading-xml-files-with-qt/ for reading plist (it is xml)