// A sample scenario for VODSim
//
0h0m0s: simulation set random seed time
0h0m0s: simulation set id time
//
//
0h0m0s: simulation set folder input/bittorrent/torrent-data
0h0m0s: simulation set file input/bittorrent/torrent-data/10MB-full.dat.torrent
0h0m0s: topology set file input/bittorrent/topology/dfn-like.brite
0h0m0s: topology add tracker
0h0m0s: topology set delays min 0 max 0
//
//
0h0m0s: topology add clients count 1 type point-to-point uplink 100Mbps downlink 100Mbps delay 0ms
0h0m0s: client 1 join group CDN
0h0m0s: topology add clients count 4 type point-to-point uplink 50Mbps downlink 100Mbps delay 0ms
0h0m0s: clients 2 to 5 join group PCDN
//0h0m0s: group CDN leave group PCDN
0h0m0s: topology add clients count 20 type point-to-point uplink 448kbps downlink 10Mbps delay 0ms
0h0m0s: clients 6 to 25 join group leechers
0h0m0s: clients 6 to 11 set streamhash 2
0h0m0s: clients 12 to 25 set streamhash 4
0h0m0s: tracker set streamhash 2
0h0m0s: tracker set streamhash 4

//0h0m0s: group PCDN leave group leechers
//0h0m0s: group CDN leave group leechers
//
//
0h0m0s: group PCDN set initial bitfield full
0h0m0s: group leechers set initial bitfield empty
//
//
0h0m0s: all clients set block size request 16384 // e.g., 524288 262144 131072 32768 16384
0h0m0s: all clients set block size send 16384 // e.g., 16384
// 0h0m0s: all clients set piece max requests 1
// 0h0m0s: all clients set concurrent requests 8
0h0m0s: all clients set piece timeout 0h0m30s
//
//
0h0m0s: all clients set autoconnect 1
0h0m0s: all clients set peers 30
0h0m0s: all clients set max peers 55
//
//
0h0m0s: group PCDN set unchoked peers 3
0h0m0s: group CDN set clienttype CDN
0h0m0s: group PCDN set clienttype PCDN
0h0m0s: group leechers set clienttype CLIENT
//
//
0h0m0s: all clients set strategy live-streaming-box
0h0m0s: group leechers set autoplay 0
// The following four lines have no effect if VOD simulation is deactivated
// (e.g., when autoplay is off and "play" is never requested).
0h0m0s: group leechers set video skip inactive
0h0m0s: group leechers set video skip tolerance 0h0m2s
0h0m0s: group leechers set video skip afterwards 0h0m15s
0h0m0s: all clients set video prebuffering 0h0m15s
//
//
0h0m0s: tracker set update interval 0h0m60s
0h0m0s: tracker set strategy treeFirst
//
//
0h0m0s: group PCDN init
0h0m0s: group CDN init
0h0m5s: group leechers init
//
// 0h0m16s: client 6 leave cloud
// 0h0m26s: client 6 rejoin cloud
