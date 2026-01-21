---
layout: default
title: Flashstrike Documentation
nav_order: 1
---

# Flashstrike — High-Performance Matching Engine Documentation

Welcome to the official documentation for **Flashstrike**, an ultra-low-latency, memory-deterministic matching engine designed for HFT and crypto exchanges.

⚠️ **Note:**  
This repository contains **documentation only**.  
The production source code remains private.

---

## 📚 Documentation Index

### [Flashstrike Matching Engine — Top-Level Architecture Overview](docs/architecture/matching_engine_overview.md)

### Core Components

| Document | Description |
|----------|---------|
| [Manager](docs/architecture/matching_engine/manager.md) | Top-level orchestrator of the Flashstrike matching engine. |
| [OrderBook](docs/architecture/matching_engine/order_book.md) | Preallocated data structure that stores all resting orders |
| [OrderPool](docs/architecture/matching_engine/order_pool.md) | High-performance, preallocated memory subsystem for orders |
| [OrderIdMap](docs/architecture/matching_engine/order_id_map.md) | High‑performance, fixed‑size, open‑addressing hash map |
| [PriceLevelStore](docs/architecture/matching_engine/price_level_store.md) | Organizes all price levels using a partitioned, bitmap‑indexed layout |
| [Partitions & PartitionPool](docs/architecture/matching_engine/partitions.md) | They provide a deterministic memory layout with constant-time price-to-level mapping |
| [Telemetry System](docs/architecture/matching_engine/telemetry.md) | HFT‑grade ultra‑low‑overhead metrics |

### Flashstrike WAL System

| Document | Description |
|----------|---------|
| [WAL Storage Architecture — Overview](docs/architecture/wal/segment_overview.md) | Provides a high-level overview of the WAL storage model. |
| [WAL Recorder System — Architectural Overview](docs/architecture/wal/recorder_overview.md) | Flashstrike Write‑Ahead Log (WAL) Recorder System |
| [WAL Recovery System — Architectural Overview](docs/architecture/wal/recorder_overview.md) | Flashstrike Write‑Ahead Log (WAL) Recovery System |

---

## 🎯 Purpose

This documentation is intended to:

- Showcase the design of a production-grade matching engine  
- Highlight low-latency techniques and data-structure decisions  
- Demonstrate systems engineering expertise for prospective employers  
- Provide a structured, browsable architecture reference  

The underlying engine implementation is **closed-source**.

---

## ⚡ Benchmark & Performance Report

Flashstrike includes a public benchmark report demonstrating the engine’s
ultra-low-latency behavior under exchange-scale workloads.

📈 51 billion operations executed end-to-end
⚡ 8.47 million requests/second sustained
🎯 Sub-microsecond median & p99 latency
🏆 HFT-grade deterministic tail behavior

👉 Read the full report:
[High-Performance Benchmark Report](docs/benchmarks/benchmark_report.md)

![System Status](docs/benchmarks/plots/00.system_status.png)

### This report is intended to showcase:
- real-world performance characteristics,
- data structure efficiency,
- tail-latency control,
- and the engine’s suitability for HFT-class workloads.

------

Happy reading!  
— *R. Lopez Caballero*


---

