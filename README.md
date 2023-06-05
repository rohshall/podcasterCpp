# podcaster
A podcast downloader written in C++

## Features
1. Reads TOML file in the home directory `~/.podcasts.toml`  to get the config about which podcasts to download, and where to store the episodes. A sample TOML file `sample-podcasts.toml` is included.
2. Does not download any episode unless specifically instructed to.

## library dependencies
1. pugixml (https://github.com/zeux/pugixml) for parsing RSS feeds
2. tomlplusplus (https://github.com/marzer/tomlplusplus) for parsing the TOML config

## Building
```
clang++ app.cpp -std=c++17 -L /usr/pkg/lib -I /usr/pkg/include/ -lpugixml -lcurl -Wall
```

### Coming soon

1. Add cmake build
2. Support command switches
3. Show listing - local and remote episodes
4. Download episodes using chapter numbers
5. Cleanup episodes.
6. Play the episode

## Usage
```
<program-name> <podcastId>
```
For example:
```
podcaster TheBulwark
```

It will download the latest episode of the "TheBulwark" podcast.


