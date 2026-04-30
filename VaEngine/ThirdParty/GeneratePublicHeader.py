import os


def update_inner_dependencies(project_dir: str) -> None:
    """
    Public/ 하위 디렉토리의 .h 파일을 스캔하여 InnerDependencies.h를 갱신.
    변경이 없으면 파일을 건드리지 않음 (증분 빌드 보호).
    """
    public_dir = os.path.join(project_dir, "Public")
    inner_h_path = os.path.join(public_dir, "InnerDependencies.h")

    if not os.path.exists(inner_h_path):
        print(f"Warning: {inner_h_path} not found.")
        return

    with open(inner_h_path, 'r', encoding='utf-8') as f:
        old_content = f.read()

    # Public/ 하위 디렉토리별 헤더 수집
    expected_sections = {}
    for item in os.listdir(public_dir):
        subdir_path = os.path.join(public_dir, item)
        if os.path.isdir(subdir_path):
            headers = [f for f in os.listdir(subdir_path) if f.endswith('.h')]
            if headers:
                headers.sort()
                expected_sections[item] = [f"{item}/{h}" for h in headers]

    # 기존 섹션 파싱
    existing_sections = []
    lines = old_content.splitlines(keepends=True)
    i = 0
    pragma_lines = []
    while i < len(lines):
        line = lines[i].rstrip('\n')
        if line.strip().startswith('#pragma once'):
            pragma_lines.append(line + '\n')
            i += 1
            continue
        stripped = line.strip()
        if stripped.startswith('//') and len(stripped) > 2 and '/' not in stripped[2:].strip():
            section_name = stripped[2:].strip()
            includes = set()
            i += 1
            while i < len(lines):
                inc_line = lines[i].rstrip('\n').strip()
                if inc_line.startswith('#include "') and inc_line.endswith('"'):
                    path = inc_line[10:-1].strip()
                    if path:
                        includes.add(path)
                if inc_line.startswith('//') and len(inc_line) > 2 and '/' not in inc_line[2:].strip():
                    break
                i += 1
            existing_sections.append((section_name, includes))
        else:
            i += 1

    # 새 콘텐츠 빌드
    new_content = ''.join(pragma_lines)
    if not new_content.endswith('\n\n'):
        new_content += '\n\n'

    for section_name, existing_includes in existing_sections:
        expected_includes = expected_sections.get(section_name, [])
        all_includes = sorted(set(existing_includes) | set(expected_includes))
        new_content += f"// {section_name}\n"
        for inc_path in all_includes:
            new_content += f'#include "{inc_path}"\n'
        new_content += "\n"

    new_sections = sorted([name for name in expected_sections if name not in dict(existing_sections)])
    for section_name in new_sections:
        new_content += f"// {section_name}\n"
        for inc_path in sorted(expected_sections[section_name]):
            new_content += f'#include "{inc_path}"\n'
        new_content += "\n"

    new_content = new_content.rstrip() + '\n'

    if new_content == old_content:
        print(f"No changes in {inner_h_path}.")
        return

    with open(inner_h_path, 'w', encoding='utf-8') as f:
        f.write(new_content)

    added = len(new_sections)
    if added > 0:
        print(f"Updated {inner_h_path}: Added {added} new section(s).")
    else:
        print(f"Updated {inner_h_path}: Fixed includes.")


def create_missing_cpp_files(project_dir: str) -> None:
    """
    Public/ 하위 디렉토리의 .h 파일을 보고, Private/ 에 대응하는 .cpp 가 없으면 생성.
    I~~ (순수 가상 인터페이스) 파일은 건너뜀.
    """
    public_dir = os.path.join(project_dir, "Public")
    private_dir = os.path.join(project_dir, "Private")
    new_cpp_paths = []

    if not os.path.exists(public_dir):
        print(f"Warning: {public_dir} not found.")
        return

    for subdir in os.listdir(public_dir):
        public_subdir = os.path.join(public_dir, subdir)
        if not os.path.isdir(public_subdir):
            continue

        private_subdir = os.path.join(private_dir, subdir)
        os.makedirs(private_subdir, exist_ok=True)

        headers = [f for f in os.listdir(public_subdir) if f.endswith('.h')]
        for header in headers:
            filename = header[:-2]

            # I~~ 순수 가상 인터페이스 건너뜀
            if filename.startswith('I') and len(filename) > 1 and filename[1].isupper():
                print(f"Skipped interface: {header}")
                continue

            cpp_filename = filename + '.cpp'
            cpp_path = os.path.join(private_subdir, cpp_filename)

            if not os.path.exists(cpp_path):
                cpp_content = f'#include "pch.h"\n\n{filename}::{filename}() {{}}\n'
                with open(cpp_path, 'w', encoding='utf-8') as f:
                    f.write(cpp_content)
                rel_path = os.path.relpath(cpp_path, project_dir).replace('\\', '/')
                new_cpp_paths.append(rel_path)
                print(f"Created: {cpp_path}")

    print(f"Created {len(new_cpp_paths)} new definition file(s).")
    if new_cpp_paths:
        add_generated_cpp_to_vcxproj(project_dir, new_cpp_paths)


from xml.dom import minidom

def add_generated_cpp_to_vcxproj(project_dir: str, new_cpp_paths: list) -> None:
    """
    생성된 .cpp 파일을 Engine.vcxproj 의 <ItemGroup Label="Definitions"> 에 추가.
    """
    vcxproj_path = os.path.join(project_dir, "Engine.vcxproj")

    doc = minidom.parse(vcxproj_path)
    root = doc.documentElement

    target_group = None
    for ig in root.getElementsByTagName('ItemGroup'):
        if ig.getAttribute('Label') == 'Definitions':
            target_group = ig
            break

    if not target_group:
        print("No ItemGroup Label='Definitions' found in Engine.vcxproj.")
        return

    existing_in_group = set()
    for cl in target_group.getElementsByTagName('ClCompile'):
        inc = cl.getAttribute('Include')
        if inc:
            existing_in_group.add(os.path.normpath(inc).replace('\\', '/'))

    added = 0
    for rel_path in new_cpp_paths:
        norm_path = os.path.normpath(rel_path).replace('/', '\\')
        if norm_path.replace('\\', '/') not in existing_in_group:
            cl_node = doc.createElement('ClCompile')
            cl_node.setAttribute('Include', norm_path)
            target_group.appendChild(cl_node)
            added += 1
            print(f"Added to Definitions: {norm_path}")

    if added > 0:
        pretty_xml = doc.toprettyxml(indent='  ')
        clean_xml = pretty_xml.replace('<?xml version="1.0" ?>', '<?xml version="1.0" encoding="utf-8"?>')
        lines = [line for line in clean_xml.splitlines() if line.strip() or line.startswith('<?')]
        final_xml = '\n'.join(lines) + '\n'

        with open(vcxproj_path, 'w', encoding='utf-8') as f:
            f.write(final_xml)
        print(f"{vcxproj_path} updated (+{added}).")
    else:
        print("No new entries to add.")
