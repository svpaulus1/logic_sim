import os

def convert_code_to_tex():
    # 1. Define the target directory safely
    # This replaces os.getcwd() + "/chaps/code" to be cross-platform safe
    f = os.path.join(os.getcwd(), "chaps", "code")
    
    # 2. Define and create the output directory
    tex_dir = os.path.join(f, "tex_code")
    os.makedirs(tex_dir, exist_ok=True) # Creates the folder if it doesn't exist
    
    # 3. Define the file extensions we want to process
    target_extensions = {".cpp", ".ml", ".py"}
    
    # 4. Loop through the files in the directory 'f'
    for filename in os.listdir(f):
        file_path = os.path.join(f, filename)
        
        # Skip directories (like our tex_code folder)
        if os.path.isdir(file_path):
            continue
            
        # Get the name and extension of the file
        name, ext = os.path.splitext(filename)
        
        # If it's one of our target code files, process it
        if ext in target_extensions:
            # Read the original code
            with open(file_path, 'r', encoding='utf-8') as code_file:
                code_content = code_file.read()
            
            # Create the new .tex filename
            # Note: I added the original extension to the name (e.g., script_py.tex)
            # to prevent files like main.cpp and main.py from overwriting each other.
            tex_filename = f"{name}_{ext.strip('.')}.tex"
            tex_filepath = os.path.join(tex_dir, tex_filename)
            
            # Write out to the new .tex file
            with open(tex_filepath, 'w', encoding='utf-8') as tex_file:
                # Wrapping it in a verbatim environment so LaTeX formats it as code
                tex_file.write("\\begin{console}\n")
                tex_file.write(code_content)
                tex_file.write("\n\\end{console}\n")
                
            print(f"Converted {filename} -> {tex_filename}")

if __name__ == "__main__":
    convert_code_to_tex()
    print("Conversion complete!")  
