# SPDX-License-Identifier: GPL-3.0-only
"""Lossless outer coding for the boot's LZ4 stream; no change to running code."""
import collections
import heapq
import struct


def pack(src):
    if len(src) > 65536:
        raise ValueError("boot stream exceeds scratch bound")
    counts = collections.Counter(src)
    if len(counts) == 1:
        counts[(next(iter(counts)) + 1) & 255] = 0
    heap = [(n, value, value) for value, n in sorted(counts.items())]
    heapq.heapify(heap)
    serial = 256
    while len(heap) > 1:
        a, b = heapq.heappop(heap), heapq.heappop(heap)
        heapq.heappush(heap, (a[0] + b[0], serial, (a[2], b[2])))
        serial += 1
    nodes, codes = [], {}

    def visit(node, code=0, depth=0):
        if isinstance(node, int):
            codes[node] = (code, depth)
            return 256 + node
        index = len(nodes)
        nodes.append(None)
        left = visit(node[0], code << 1, depth + 1)
        right = visit(node[1], code << 1 | 1, depth + 1)
        nodes[index] = (left, right)
        return index

    if heap:
        visit(heap[0][2])
    payload, word, bits = bytearray(), 0, 0
    for value in src:
        code, length = codes[value]
        word = word << length | code
        bits += length
        while bits >= 8:
            bits -= 8
            payload.append(word >> bits & 255)
        word &= (1 << bits) - 1
    if bits:
        payload.append(word << (8 - bits))
    return (b"HF\x01" + bytes((len(nodes),)) + struct.pack("<II", len(src), len(payload)) +
            b"".join(struct.pack("<HH", *node) for node in nodes) + payload)


def unpack(src):
    if len(src) < 12 or src[:3] != b"HF\x01":
        raise ValueError("boot Huffman header")
    nodes = src[3]
    size, packed = struct.unpack_from("<II", src, 4)
    start = 12 + 4 * nodes
    if size > 65536 or packed > 65536 or start + packed != len(src) or (size and not nodes):
        raise ValueError("boot Huffman bounds")
    out, bit = bytearray(), 0
    while len(out) < size:
        node, depth = 0, 0
        while node < 256:
            if node >= nodes or bit >= packed * 8 or depth >= 32:
                raise ValueError("boot Huffman tree or stream")
            side = src[start + bit // 8] >> (7 - bit % 8) & 1
            node = struct.unpack_from("<H", src, 12 + node * 4 + side * 2)[0]
            bit += 1
            depth += 1
        if node > 511:
            raise ValueError("boot Huffman symbol")
        out.append(node - 256)
    return bytes(out)
