# Nasdaq TotalView-ITCH 5.0 schema audit

## Scope

This document records the source mapping for the
[Nasdaq TotalView-ITCH 5.0 schema](../schemas/nasdaq/totalview_itch_5_0.toml) and
its 23 fixture manifests. The schema and fixtures derive from the locked
official Nasdaq TotalView-ITCH 5.0 and BinaryFILE PDFs. This mapping is not
exchange certification.

The mapping and fixture expectations received an independent line-by-line
review against the locked sources on `2026-07-14`; no discrepancies were found.

The mapping covers all 23 messages represented by the schema. Field names are
the deterministic snake-case source names used by FeedForge; the official
name, offset, width, and type come directly from the cited Nasdaq table. Page
numbers below are the printed page numbers in the PDF, not the PDF reader's
one-based sheet index.

## Authoritative source evidence

The locked source set contains the official documents fetched on `2026-07-14`
from these URLs:

| Source | Official URL | Version/revision evidence | SHA-256 | Bytes |
|---|---|---|---|---:|
| Nasdaq TotalView-ITCH | <https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf> | Version 5.0; latest Appendix A entry is 2023-04-28. PDF metadata creation/modification is 2024-02-28 12:21:43 UTC. | `45e0531d1b4b3beb886e9618b2ab824a5aa9bda3a99c0dff03509306e68aacc3` | 1,200,722 |
| Nasdaq BinaryFILE | <https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/binaryfile.pdf> | Cover says Version 1.00; latest revision-history entry is 2010-03-30. PDF metadata is 2010-03-31 01:04:37 BST. | `a1f443400728b3ce44953e9ae263e4846fe6ad68420e7a635829872aefdfff60` | 84,384 |

The immutable retrieval metadata is recorded in the
[source lock](../schemas/sources.lock.toml). This mapping uses the official ITCH
field tables on printed pages 4-20 and the general Data Types rules on printed
page 4. BinaryFILE page 1 states that each record is a two-byte big-endian
payload length followed by the payload; page 1 also defines a zero-length
record as end-of-session and absence of that record as incomplete. No community
schema is a source.

## Message and fixture inventory

| Type | Schema message | Bytes | Official table | Positive fixture |
|---|---|---:|---|---|
| `S` | `system_event` | 12 | §1.1 p.4 | [`01_system_event.toml`](../tests/fixtures/itch50/01_system_event.toml) |
| `R` | `stock_directory` | 39 | §1.2.1 pp.5-8 | [`02_stock_directory.toml`](../tests/fixtures/itch50/02_stock_directory.toml) |
| `H` | `stock_trading_action` | 25 | §1.2.2 p.8 | [`03_stock_trading_action.toml`](../tests/fixtures/itch50/03_stock_trading_action.toml) |
| `Y` | `reg_sho_restriction` | 20 | §1.2.3 p.9 | [`04_reg_sho_restriction.toml`](../tests/fixtures/itch50/04_reg_sho_restriction.toml) |
| `L` | `market_participant_position` | 26 | §1.2.4 pp.9-10 | [`05_market_participant_position.toml`](../tests/fixtures/itch50/05_market_participant_position.toml) |
| `V` | `mwcb_decline_level` | 35 | §1.2.5.1 p.10 | [`06_mwcb_decline_level.toml`](../tests/fixtures/itch50/06_mwcb_decline_level.toml) |
| `W` | `mwcb_status` | 12 | §1.2.5.2 pp.10-11 | [`07_mwcb_status.toml`](../tests/fixtures/itch50/07_mwcb_status.toml) |
| `K` | `ipo_quoting_period_update` | 28 | §1.2.6 p.11 | [`08_ipo_quoting_period_update.toml`](../tests/fixtures/itch50/08_ipo_quoting_period_update.toml) |
| `J` | `luld_auction_collar` | 35 | §1.2.7 p.12 | [`09_luld_auction_collar.toml`](../tests/fixtures/itch50/09_luld_auction_collar.toml) |
| `h` | `operational_halt` | 21 | §1.2.8 p.12 | [`10_operational_halt.toml`](../tests/fixtures/itch50/10_operational_halt.toml) |
| `A` | `add_order` | 36 | §1.3.1 p.13 | [`11_add_order.toml`](../tests/fixtures/itch50/11_add_order.toml) |
| `F` | `add_order_mpid` | 40 | §1.3.2 p.13 | [`12_add_order_mpid.toml`](../tests/fixtures/itch50/12_add_order_mpid.toml) |
| `E` | `order_executed` | 31 | §1.4.1 p.14 | [`13_order_executed.toml`](../tests/fixtures/itch50/13_order_executed.toml) |
| `C` | `order_executed_with_price` | 36 | §1.4.2 pp.14-15 | [`14_order_executed_with_price.toml`](../tests/fixtures/itch50/14_order_executed_with_price.toml) |
| `X` | `order_cancel` | 23 | §1.4.3 p.15 | [`15_order_cancel.toml`](../tests/fixtures/itch50/15_order_cancel.toml) |
| `D` | `order_delete` | 19 | §1.4.4 p.15 | [`16_order_delete.toml`](../tests/fixtures/itch50/16_order_delete.toml) |
| `U` | `order_replace` | 35 | §1.4.5 pp.15-16 | [`17_order_replace.toml`](../tests/fixtures/itch50/17_order_replace.toml) |
| `P` | `trade` | 44 | §1.5.1 pp.16-17 | [`18_trade.toml`](../tests/fixtures/itch50/18_trade.toml) |
| `Q` | `cross_trade` | 40 | §1.5.2 p.17 | [`19_cross_trade.toml`](../tests/fixtures/itch50/19_cross_trade.toml) |
| `B` | `broken_trade` | 19 | §1.5.3 p.18 | [`20_broken_trade.toml`](../tests/fixtures/itch50/20_broken_trade.toml) |
| `I` | `net_order_imbalance_indicator` | 50 | §1.6 pp.18-19 | [`21_net_order_imbalance_indicator.toml`](../tests/fixtures/itch50/21_net_order_imbalance_indicator.toml) |
| `N` | `retail_price_improvement_indicator` | 20 | §1.7 pp.19-20 | [`22_retail_price_improvement_indicator.toml`](../tests/fixtures/itch50/22_retail_price_improvement_indicator.toml) |
| `O` | `direct_listing_with_capital_raise` | 48 | §1.8 p.20 | [`23_direct_listing_with_capital_raise.toml`](../tests/fixtures/itch50/23_direct_listing_with_capital_raise.toml) |

Every fixture contains hand-authored readable hex, all expected fields, the
expected `all_messages` event, the exact `order_events` emit/skip result, and
deterministic size-minus-one/size-plus-one derivations.

## Type interpretation

`uint16`, `uint32`, `uint48`, and `uint64` below are unsigned big-endian wire
integers. `alpha(N)` is fixed-width ASCII with trailing spaces preserved.
`Price(4)` and `Price(8)` are unsigned fixed-point integers with scales 4 and 8
respectively, following the Nasdaq Data Types rule on p.4. The logical/C++
mapping is:

- `timestamp_ns` -> `feedforge::timestamp_ns`;
- `stock_locate`, `tracking_number`, `order_reference_number`, `match_number`,
  and 32-bit `share_count` -> the corresponding strong FeedForge type;
- `Price(4)` -> `feedforge::decimal<std::uint32_t, 4>`;
- `Price(8)` -> `feedforge::decimal<std::uint64_t, 8>`;
- `raw32`/`raw64` -> `std::uint32_t`/`std::uint64_t`;
- `ascii<N>` -> `feedforge::ascii<N>`;
- `reserved` -> complete wire coverage with no projected member.

## Field-by-field mapping

### `S` — `system_event` (12 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `S` | ascii<1>, discriminator | — | §1.1 p.4 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.1 p.4 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.1 p.4 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.1 p.4 |
| `event_code` | 11 | 1 | alpha(1) | ascii<1> | — | §1.1 p.4 |

### `R` — `stock_directory` (39 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `R` | ascii<1>, discriminator | — | §1.2.1 p.5 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.2.1 p.5 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.2.1 p.5 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.2.1 p.5 |
| `stock` | 11 | 8 | alpha(8) | ascii<8> | — | §1.2.1 p.5 |
| `market_category` | 19 | 1 | alpha(1) | ascii<1> | — | §1.2.1 p.5 |
| `financial_status_indicator` | 20 | 1 | alpha(1) | ascii<1> | — | §1.2.1 p.6 |
| `round_lot_size` | 21 | 4 | uint32 | share_count | — | §1.2.1 p.6 |
| `round_lots_only` | 25 | 1 | alpha(1) | ascii<1> | — | §1.2.1 p.6 |
| `issue_classification` | 26 | 1 | alpha(1) | ascii<1> | — | §1.2.1 p.6 |
| `issue_sub_type` | 27 | 2 | alpha(2) | ascii<2> | — | §1.2.1 p.6 |
| `authenticity` | 29 | 1 | alpha(1) | ascii<1> | — | §1.2.1 p.6 |
| `short_sale_threshold_indicator` | 30 | 1 | alpha(1) | ascii<1> | — | §1.2.1 p.7 |
| `ipo_flag` | 31 | 1 | alpha(1) | ascii<1> | — | §1.2.1 p.7 |
| `luld_reference_price_tier` | 32 | 1 | alpha(1) | ascii<1> | — | §1.2.1 p.7 |
| `etp_flag` | 33 | 1 | alpha(1) | ascii<1> | — | §1.2.1 p.7 |
| `etp_leverage_factor` | 34 | 4 | uint32 | raw32 | — | §1.2.1 p.7 |
| `inverse_indicator` | 38 | 1 | alpha(1) | ascii<1> | — | §1.2.1 p.8 |

### `H` — `stock_trading_action` (25 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `H` | ascii<1>, discriminator | — | §1.2.2 p.8 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.2.2 p.8 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.2.2 p.8 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.2.2 p.8 |
| `stock` | 11 | 8 | alpha(8) | ascii<8> | — | §1.2.2 p.8 |
| `trading_state` | 19 | 1 | alpha(1) | ascii<1> | — | §1.2.2 p.8 |
| `reserved` | 20 | 1 | reserved(1) | no projected member | — | §1.2.2 p.8 |
| `reason` | 21 | 4 | alpha(4) | ascii<4> | — | §1.2.2 p.8 |

### `Y` — `reg_sho_restriction` (20 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `Y` | ascii<1>, discriminator | — | §1.2.3 p.9 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.2.3 p.9 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.2.3 p.9 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.2.3 p.9 |
| `stock` | 11 | 8 | alpha(8) | ascii<8> | — | §1.2.3 p.9 |
| `reg_sho_action` | 19 | 1 | alpha(1) | ascii<1> | — | §1.2.3 p.9 |

### `L` — `market_participant_position` (26 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `L` | ascii<1>, discriminator | — | §1.2.4 p.9 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.2.4 p.9 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.2.4 p.9 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.2.4 p.9 |
| `mpid` | 11 | 4 | alpha(4) | ascii<4> | — | §1.2.4 p.10 |
| `stock` | 15 | 8 | alpha(8) | ascii<8> | — | §1.2.4 p.10 |
| `primary_market_maker` | 23 | 1 | alpha(1) | ascii<1> | — | §1.2.4 p.10 |
| `market_maker_mode` | 24 | 1 | alpha(1) | ascii<1> | — | §1.2.4 p.10 |
| `market_participant_state` | 25 | 1 | alpha(1) | ascii<1> | — | §1.2.4 p.10 |

### `V` — `mwcb_decline_level` (35 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `V` | ascii<1>, discriminator | — | §1.2.5.1 p.10 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.2.5.1 p.10 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.2.5.1 p.10 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.2.5.1 p.10 |
| `level_1` | 11 | 8 | Price(8) | decimal<uint64,8> | 8 | §1.2.5.1 p.10 |
| `level_2` | 19 | 8 | Price(8) | decimal<uint64,8> | 8 | §1.2.5.1 p.10 |
| `level_3` | 27 | 8 | Price(8) | decimal<uint64,8> | 8 | §1.2.5.1 p.10 |

### `W` — `mwcb_status` (12 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `W` | ascii<1>, discriminator | — | §1.2.5.2 p.11 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.2.5.2 p.11 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.2.5.2 p.11 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.2.5.2 p.11 |
| `breached_level` | 11 | 1 | alpha(1) | ascii<1> | — | §1.2.5.2 p.11 |

### `K` — `ipo_quoting_period_update` (28 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `K` | ascii<1>, discriminator | — | §1.2.6 p.11 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.2.6 p.11 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.2.6 p.11 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.2.6 p.11 |
| `stock` | 11 | 8 | alpha(8) | ascii<8> | — | §1.2.6 p.11 |
| `ipo_quotation_release_time` | 19 | 4 | uint32 | raw32 | — | §1.2.6 p.11 |
| `ipo_quotation_release_qualifier` | 23 | 1 | alpha(1) | ascii<1> | — | §1.2.6 p.11 |
| `ipo_price` | 24 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.2.6 p.11 |

### `J` — `luld_auction_collar` (35 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `J` | ascii<1>, discriminator | — | §1.2.7 p.12 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.2.7 p.12 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.2.7 p.12 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.2.7 p.12 |
| `stock` | 11 | 8 | alpha(8) | ascii<8> | — | §1.2.7 p.12 |
| `auction_collar_reference_price` | 19 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.2.7 p.12 |
| `upper_auction_collar_price` | 23 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.2.7 p.12 |
| `lower_auction_collar_price` | 27 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.2.7 p.12 |
| `auction_collar_extension` | 31 | 4 | uint32 | raw32 | — | §1.2.7 p.12 |

### `h` — `operational_halt` (21 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `h` | ascii<1>, discriminator | — | §1.2.8 p.12 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.2.8 p.12 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.2.8 p.12 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.2.8 p.12 |
| `stock` | 11 | 8 | alpha(8) | ascii<8> | — | §1.2.8 p.12 |
| `market_code` | 19 | 1 | alpha(1) | ascii<1> | — | §1.2.8 p.12 |
| `operational_halt_action` | 20 | 1 | alpha(1) | ascii<1> | — | §1.2.8 p.12 |

### `A` — `add_order` (36 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `A` | ascii<1>, discriminator | — | §1.3.1 p.13 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.3.1 p.13 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.3.1 p.13 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.3.1 p.13 |
| `order_reference_number` | 11 | 8 | uint64 | order_reference_number | — | §1.3.1 p.13 |
| `buy_sell_indicator` | 19 | 1 | alpha(1) | ascii<1> | — | §1.3.1 p.13 |
| `shares` | 20 | 4 | uint32 | share_count | — | §1.3.1 p.13 |
| `stock` | 24 | 8 | alpha(8) | ascii<8> | — | §1.3.1 p.13 |
| `price` | 32 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.3.1 p.13 |

### `F` — `add_order_mpid` (40 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `F` | ascii<1>, discriminator | — | §1.3.2 p.13 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.3.2 p.13 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.3.2 p.13 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.3.2 p.13 |
| `order_reference_number` | 11 | 8 | uint64 | order_reference_number | — | §1.3.2 p.13 |
| `buy_sell_indicator` | 19 | 1 | alpha(1) | ascii<1> | — | §1.3.2 p.13 |
| `shares` | 20 | 4 | uint32 | share_count | — | §1.3.2 p.13 |
| `stock` | 24 | 8 | alpha(8) | ascii<8> | — | §1.3.2 p.13 |
| `price` | 32 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.3.2 p.13 |
| `attribution` | 36 | 4 | alpha(4) | ascii<4> | — | §1.3.2 p.13 |

### `E` — `order_executed` (31 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `E` | ascii<1>, discriminator | — | §1.4.1 p.14 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.4.1 p.14 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.4.1 p.14 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.4.1 p.14 |
| `order_reference_number` | 11 | 8 | uint64 | order_reference_number | — | §1.4.1 p.14 |
| `executed_shares` | 19 | 4 | uint32 | share_count | — | §1.4.1 p.14 |
| `match_number` | 23 | 8 | uint64 | match_number | — | §1.4.1 p.14 |

### `C` — `order_executed_with_price` (36 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `C` | ascii<1>, discriminator | — | §1.4.2 p.14 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.4.2 p.14 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.4.2 p.14 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.4.2 p.15 |
| `order_reference_number` | 11 | 8 | uint64 | order_reference_number | — | §1.4.2 p.15 |
| `executed_shares` | 19 | 4 | uint32 | share_count | — | §1.4.2 p.15 |
| `match_number` | 23 | 8 | uint64 | match_number | — | §1.4.2 p.15 |
| `printable` | 31 | 1 | alpha(1) | ascii<1> | — | §1.4.2 p.15 |
| `execution_price` | 32 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.4.2 p.15 |

### `X` — `order_cancel` (23 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `X` | ascii<1>, discriminator | — | §1.4.3 p.15 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.4.3 p.15 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.4.3 p.15 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.4.3 p.15 |
| `order_reference_number` | 11 | 8 | uint64 | order_reference_number | — | §1.4.3 p.15 |
| `cancelled_shares` | 19 | 4 | uint32 | share_count | — | §1.4.3 p.15 |

### `D` — `order_delete` (19 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `D` | ascii<1>, discriminator | — | §1.4.4 p.15 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.4.4 p.15 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.4.4 p.15 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.4.4 p.15 |
| `order_reference_number` | 11 | 8 | uint64 | order_reference_number | — | §1.4.4 p.15 |

### `U` — `order_replace` (35 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `U` | ascii<1>, discriminator | — | §1.4.5 p.16 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.4.5 p.16 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.4.5 p.16 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.4.5 p.16 |
| `original_order_reference_number` | 11 | 8 | uint64 | order_reference_number | — | §1.4.5 p.16 |
| `new_order_reference_number` | 19 | 8 | uint64 | order_reference_number | — | §1.4.5 p.16 |
| `shares` | 27 | 4 | uint32 | share_count | — | §1.4.5 p.16 |
| `price` | 31 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.4.5 p.16 |

### `P` — `trade` (44 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `P` | ascii<1>, discriminator | — | §1.5.1 p.16 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.5.1 p.16 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.5.1 p.16 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.5.1 p.16 |
| `order_reference_number` | 11 | 8 | uint64 | order_reference_number | — | §1.5.1 p.16 |
| `buy_sell_indicator` | 19 | 1 | alpha(1) | ascii<1> | — | §1.5.1 p.17 |
| `shares` | 20 | 4 | uint32 | share_count | — | §1.5.1 p.17 |
| `stock` | 24 | 8 | alpha(8) | ascii<8> | — | §1.5.1 p.17 |
| `price` | 32 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.5.1 p.17 |
| `match_number` | 36 | 8 | uint64 | match_number | — | §1.5.1 p.17 |

### `Q` — `cross_trade` (40 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `Q` | ascii<1>, discriminator | — | §1.5.2 p.17 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.5.2 p.17 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.5.2 p.17 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.5.2 p.17 |
| `shares` | 11 | 8 | uint64 | raw64 | — | §1.5.2 p.17 |
| `stock` | 19 | 8 | alpha(8) | ascii<8> | — | §1.5.2 p.17 |
| `cross_price` | 27 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.5.2 p.17 |
| `match_number` | 31 | 8 | uint64 | match_number | — | §1.5.2 p.17 |
| `cross_type` | 39 | 1 | alpha(1) | ascii<1> | — | §1.5.2 p.17 |

### `B` — `broken_trade` (19 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `B` | ascii<1>, discriminator | — | §1.5.3 p.18 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.5.3 p.18 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.5.3 p.18 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.5.3 p.18 |
| `match_number` | 11 | 8 | uint64 | match_number | — | §1.5.3 p.18 |

### `I` — `net_order_imbalance_indicator` (50 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `I` | ascii<1>, discriminator | — | §1.6 p.18 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.6 p.18 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.6 p.18 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.6 p.18 |
| `paired_shares` | 11 | 8 | uint64 | raw64 | — | §1.6 p.19 |
| `imbalance_shares` | 19 | 8 | uint64 | raw64 | — | §1.6 p.19 |
| `imbalance_direction` | 27 | 1 | alpha(1) | ascii<1> | — | §1.6 p.19 |
| `stock` | 28 | 8 | alpha(8) | ascii<8> | — | §1.6 p.19 |
| `far_price` | 36 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.6 p.19 |
| `near_price` | 40 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.6 p.19 |
| `current_reference_price` | 44 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.6 p.19 |
| `cross_type` | 48 | 1 | alpha(1) | ascii<1> | — | §1.6 p.19 |
| `price_variation_indicator` | 49 | 1 | alpha(1) | ascii<1> | — | §1.6 p.19 |

### `N` — `retail_price_improvement_indicator` (20 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `N` | ascii<1>, discriminator | — | §1.7 p.19 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.7 p.19 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.7 p.20 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.7 p.20 |
| `stock` | 11 | 8 | alpha(8) | ascii<8> | — | §1.7 p.20 |
| `interest_flag` | 19 | 1 | alpha(1) | ascii<1> | — | §1.7 p.20 |

### `O` — `direct_listing_with_capital_raise` (48 bytes)

| Field | Offset | Width | Physical | Logical/C++ | Scale | Citation |
|---|---:|---:|---|---|---:|---|
| `message_type` | 0 | 1 | alpha(1), `O` | ascii<1>, discriminator | — | §1.8 p.20 |
| `stock_locate` | 1 | 2 | uint16 | stock_locate | — | §1.8 p.20 |
| `tracking_number` | 3 | 2 | uint16 | tracking_number | — | §1.8 p.20 |
| `timestamp` | 5 | 6 | uint48 | timestamp_ns | — | §1.8 p.20 |
| `stock` | 11 | 8 | alpha(8) | ascii<8> | — | §1.8 p.20 |
| `open_eligibility_status` | 19 | 1 | alpha(1) | ascii<1> | — | §1.8 p.20 |
| `minimum_allowable_price` | 20 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.8 p.20 |
| `maximum_allowable_price` | 24 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.8 p.20 |
| `near_execution_price` | 28 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.8 p.20 |
| `near_execution_time` | 32 | 8 | uint64 | raw64 | — | §1.8 p.20 |
| `lower_price_range_collar` | 40 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.8 p.20 |
| `upper_price_range_collar` | 44 | 4 | Price(4) | decimal<uint32,4> | 4 | §1.8 p.20 |

## Coverage and pipeline checks

- Each message covers byte zero through `size - 1` with no overlap or gap.
- Every discriminator is at offset 0, width 1, and exactly matches its
  case-sensitive message type. Lowercase `h` is distinct from uppercase `H`.
- The only official reserved range is `H` offset 20 width 1; it is represented
  as `reserved`, so it is validated but not projected.
- [`all_messages.toml`](../pipelines/all_messages.toml) contains all 23 emits in
  unsigned ASCII discriminator order and uses `fields = ["*"]`.
- [`order_events.toml`](../pipelines/order_events.toml) contains exactly the
  eight projections defined by the
  [pipeline format](pipeline-format.md#canonical-v01-pipelines), in the listed
  member order.
- The 2023 `O` message is present in the schema, all-message pipeline, and a
  dedicated positive/negative fixture manifest.

## Source ambiguities and normative interpretations

1. **`O.near_execution_time` unit:** §1.8 calls this an eight-byte Integer and
   says only “the time at which the near execution price was set.” It does not
   define seconds, nanoseconds, or another unit. The schema therefore leaves it
   as raw unsigned 64-bit; no time unit is inferred.
2. **`R.etp_leverage_factor`:** Appendix A says the 2022 edit supports
   non-integer leverage factors, while the current table still declares a
   four-byte Integer and describes rounding to an integer. It remains raw
   unsigned 32-bit; no decimal scale is invented.
3. **`K.ipo_price` wording:** §1.2.6 labels the four-byte field `Price (4)` but
   its prose also contains legacy-looking space-padding language. The general
   Data Types section says integer fields are binary and price precision is
   implied. The schema therefore uses a four-byte unsigned scale-4 decimal.
4. **Table-title errors:** the `W` table is headed “MWCB Decline Level Message”
   even though §1.2.5.2, discriminator `W`, and the field notes identify the
   Status Message. The `Q` table is headed “Trade Message (Non-Cross)” even
   though §1.5.2 and discriminator `Q` identify Cross Trade. Names follow the
   section and discriminator descriptions.
5. **Pagination artifacts:** several official rows cross printed page
   boundaries (`R`, `L`, `C`, `P`, `I`, and `N`). The schema records the page
   on which each row appears; the message inventory records the complete page
   span.
6. **Allowed alpha values:** the official prose lists current code values, but
   FeedForge v0.1 preserves unknown alpha values and performs no semantic enum
   rejection. The structural schema therefore does not turn those lists into
   runtime constraints.
