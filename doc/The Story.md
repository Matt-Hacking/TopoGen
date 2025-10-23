This is just the story of how this software came to be, in case it is helpful or interesting to someone.

############
Phase 1: Script
############

A friend of mine was interested in doing a local manufacturing project whereby he would get cut out pieces of wood using a laser cutter and stack them to create the topography of a place. His products are considerably more ambitious than I am describing (this is just the first, easiest part of the process). But to give him time to focus on the hard parts, he needed a workflow that would significantly speed up the process of cutting out the layers. He wanted to use OpenStreetMaps as his data source. His workflow was to go to OSM, find the location, zoom and rotate and do whatever else, and generaet a contour map of what he wanted. Then he would have to handdraw each layer by tracing the contours. That was untenable, and he was looking for a solution. We were chatting on the phone about this and I started distracting myself because I am a lousy conversationalist.

I conceived of a script that would use curl to fetch the data from OSM, translate the GeoTIFF into a heightmap, and then... do a thing. I wasn't sure what. Eventually it would wind up being something  you could put into a slicer, use the slicer to slice it to get the polygons at each level, and then... do something with those.

############
Phase 2: Working Python version
############

The friend had come across a Reddit thread in which someone mentioned having built a slicer that sounded promising. The someone turned out to be Boris Legradic, and the promising tool was his outstanding Python laser slicer (https://github.com/borsic77/laser_slicer). I examined that project and saw it could be made to work with very little addiitonal tweaking. I used Claude Code to make the tweaks, and within about a half hour I had version 1 of the Python version of topo-gen.

I showed my friend the Python version, and he immediately saw the potential. And I saw the potential of using Claude Code. To be clear, at least as of Sonnet 4.5, Claude is NOT a good coding companion for an experienced programmer. It is slow, backtracks, does not take instruction well, and will often stray from the requirements. Not only can it not innovate effectively on its own (which is a good thing!) but it also cannot learn my innovations, idiosyncacies, and ticks. As a result, it does things wrong CONSTANTLY. For an easy example: I asked it to always use relative paths in shell commands. Among other things, this reduces the number of times it has to ask for permission: 'find ./build/ -name new_version' will be immediately executed if it has permission to use 'find' in the working directory and lower, where 'find /Users/Matthew\ Block/Working\ Files/Programming/Projects/TopoGen/build/ -name new_version' does two bad things - requires Claude to ask for permission to operate in /Users/, and gives Claude permission to operate in /Users/. So relative paths. And this is saved in user memory, project memory, and the current directory. But I nonetheless have to remind Claude to use relative paths for nearly every shell command, because it learned somewhere that absolute paths were the correct idiom.

So Claude Code's potential is NOT that it is an outstanding coding companion for a competent and experienced coder. But it DOES have access to the idioms and dictionaries of thousands of expert programmers from the last several generations. So if a person is not already an experienced programmer, or is not nuanced in a particular language, Claude Code does not improve workflows for excellent programmers, but it makes mediocre programmers into adequate programmers. I am mediocre programmer. W00t!

I started to get greedy. I thought of some features that might be cool, like being able to label each layer but, even if the label actually were to be cut by the laser, have it hidden by the next layer. I thought of ways to accomplish that, and Claude Code created Python that would execute my algorithm. It was tragically bad at coming up with its own algorithms - they tended to be twisty, arcane, inefficient, ineffective, and unamintainable. The project began to bloat pretty signficantly, but it worked.

And then I added STL output.

It should be noted that STL output is almost exactly the opposite of the reason for building this thing. STLs describe objects, which can be sliced and 3D printed. This project was supposed to create slices of objects that could be cut out of a substrate of some kind (wood. The substrate was wood). And triangulation SUCKED. I spent several days learning about, trying, and rejecting triangulation strategies and algorithms before learning that the problem was already well solved long before I came on the scene.

Also worth noting that coding, and in particular coding this project, aren't even my hobby. My hobby is woodworking, maybe with a side of 3D printing and laser etching. I have the tools to do those things. But coding this project fit in to little crevices and nooks when I wasn't doing other things.

Anyhow, at some point I felt the Python version was pushed about as far as it could be, and I was starting to run into performance bottlenecks. I know C++ better than Python, and thought it might be more performant.

###########
Phase 3: C++
###########

So I asked Claude to refactor the entire project into C++. Whcih it did. Badly.

We spent nearly as much time cleaning up and debugging the translation of the Python version into C++ as we had building the Python version. Among other things, very different libraries were available for Python than for C++. But eventually, after a week or two of late nights and Claude tokens, I felt like we had a C++ version that mostly worked as well as the Python version (although it did not yet have feature parity).

And then I started to get greedy. I started adding features, sometimes to both the C++ and the Python version, sometimes just to the C++ version. And I noticed that things weren't working that I thought had been working, so I started bug fixing. And testing. And fixing.

And finally, a few nights ago, I felt like I had done it. The C++ version compiled with no errors or warnings. It worked - it did what it said it would do in the manner I thought it should. And it had feature parity with the Python version, so the Python version was no longer better than the C++ version at anything. I felt the software was probably ready for its first public beta.

And then I started to get greedy.

##########
Phase 4: GUI
##########

So I started researching cross-platform GUI frameworks, and landed on Qt. And we started the process of creating a Qt version of the program that has feature parity with the command-line version, and that is not just a wrapper around the command-line version. It took about 4 days, but I felt like we finally had something that (although broken in places) basically worked. Good time for a public beta!

And then I started to get greedy.

##########
Phase 5: Multiarch
##########

The whole reason for building on Qt was so I could eventually release on multiple platforms. So it was time to start building packaging and distribution systems for multiple platforms. And that meant Git. Smarter people than me already know that Git greatly simplifies cross-platform development. Claude knew it, too. So here we are.
