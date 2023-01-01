#!/usr/bin/env python3
#
# (C) 2021 Magnus Feuer
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.


import setuptools

long_description="""## Sample publish / subscribe python code for sigfs."""

setuptools.setup(
    name="sigfs-example",
    version="0.0.1",
    author="Magnus Feuer",
    author_email="magnus@feuerworks.com",
    description="Signal filesystem pub/sub examples",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/magnusfeuer/sigfs",
    packages=setuptools.find_packages(),
    install_requires=[''],
    scripts=["sigfs-subscribe.py", "sigfs-publish.py" ],
    data_files=[ ],
    include_package_data=True,
    classifiers=[
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)",
        "Operating System :: Linux",
    ],
    python_requires='>=3.8',
)
