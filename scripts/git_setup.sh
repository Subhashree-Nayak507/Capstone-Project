#!/bin/sh
# Initialise the repository with the recommended branching model.
git init -b main
git checkout -b develop
echo "Branches: main (releases, tagged) <- develop (integration) <- feature/* | fix/* | docs/*"
