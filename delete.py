import os

current_directory = os.getcwd()
for filename in os.listdir(current_directory):

    if filename.endswith('.et'):

        file_path = os.path.join(current_directory, filename)
        try:
            os.remove(file_path)
            print(f"Deleted: {file_path}")
        except Exception as e:
            print(f"Failed to delete {file_path}: {e}")