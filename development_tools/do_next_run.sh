rm -f output/*
cd ..
if scripts/quick-build.sh; then cd build
	./topo-gen --force-all-layers --num-layers=10 --output-layers --output-stacked --output-formats svg,stl,png --base-name McKinley --verbose
else 
	echo "Build failed\n"
fi
