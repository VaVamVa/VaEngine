import os
import GeneratePublicHeader

if __name__ == "__main__":
    solution_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    project_dir = os.path.join(solution_dir, "Engine")

    print(f"ProjectDir: {project_dir}")
    GeneratePublicHeader.update_inner_dependencies(project_dir)
    GeneratePublicHeader.create_missing_cpp_files(project_dir)
