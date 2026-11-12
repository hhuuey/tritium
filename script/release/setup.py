from setuptools import setup, find_packages
import os

parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

setup(
    name="tritium",
    version="0.1.0",
    author="Huanghhdw",
    author_email="your_email@example.com",
    description="Tritium - Road Structure Construction and Topology Generation",
    long_description="hello world",
    long_description_content_type="text/markdown",
    url="https://github.com/huanghhdw/tritium",
    packages=find_packages(where=parent_dir, exclude=["tests", "tests.*", "release*"]),
    include_package_data=True,
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "Topic :: Software Development :: Libraries :: Python Modules",
    ],
    python_requires=">=3.8",
    install_requires=[],
    entry_points={
        "console_scripts": [
            "road-pipeline-vma=project.road_structure.road_pipeline_vma:main",
            "road-pipeline-unimap=project.road_structure.road_pipeline_unimap:main",
            "road-pipeline-huey=project.road_structure.road_pipeline_huey:main",
        ],
    },
)
