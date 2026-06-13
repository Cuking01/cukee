## Mortal 设计梳理

本文档基于相邻仓库 `../Mortal` 的当前代码整理，主要用于为 CuKee 的模型和训练设计提供参考。Mortal 是一个面向日本麻将的开源深度强化学习 AI。

### 代码入口

- 模型定义：`../Mortal/mortal/model.py`
- 主训练流程：`../Mortal/mortal/train.py`
- 数据加载：`../Mortal/mortal/dataloader.py`
- 小局奖励计算：`../Mortal/mortal/reward_calculator.py`
- GRP 训练：`../Mortal/mortal/train_grp.py`
- 动作空间和观测维度常量：`../Mortal/libriichi/src/consts.rs`
- 观测特征编码：`../Mortal/libriichi/src/state/obs_repr.rs`
- 单人手牌表/速度与 EV 相关逻辑：`../Mortal/libriichi/src/state/agent_helper.rs`、`../Mortal/libriichi/src/algo/sp/candidate.rs`

### 整体结构

Mortal 的主模型由三部分组成：

- `Brain`：把麻将观测编码成 1024 维局面表征 `phi`。
- `DQN`：基于 `phi` 和合法动作 mask 输出每个动作的 Q 值。
- `AuxNet`：基于 `phi` 输出辅助任务，目前主训练中使用的是下一小局名次分类。

此外还有一个独立训练的 `GRP` 模型，用于根据半庄进程中的局面信息估计最终名次概率。主训练通过 `GRP` 计算小局级 delta pt reward。

### 输入表示

Mortal 的输入不是事件序列 RNN，而是当前状态的二维观测张量。

`ACTION_SPACE = 46`，`version=4` 时：

- 可见观测维度：`obs_shape(4) = (1012, 34)`
- oracle 观测维度：`oracle_obs_shape(4) = (217, 34)`

`Brain` 的 ResNet 输入通道数为 1012，宽度为 34。34 对应 34 种去赤牌后的基础牌种。多数状态信息被编码成若干通道，每个通道在 34 个牌种位置上填值或 one-hot。

观测中包含大量规则特征和手工特征，例如：

- 手牌、牌河、副露、宝牌、立直状态、点数、场次等基本公开信息。
- 当前 shanten。
- 当前等待牌。
- furiten 状态。
- 可执行动作及合法动作 mask。
- 可打牌集合。
- 保持向听的打牌集合。
- 进向听的打牌集合。
- 一向听以内的无条件听牌候选。
- version 4 中的单人手牌表特征，包括单人 EV、有效牌、未来若干巡听牌概率、和牌概率和期望值。

值得注意的是，Mortal 没有把进攻速度和手牌 EV 主要作为辅助预测头，而是把这些内容直接作为输入特征提供给模型。

### 动作空间

动作空间总维度为 46：

- 37：打牌或杠选择，包含赤五相关选择。
- 1：立直。
- 3：吃，分别对应 low/mid/high。
- 1：碰。
- 1：杠决定。
- 1：和牌。
- 1：流局。
- 1：pass。

模型输出 Q 值后会使用合法动作 mask，把非法动作置为 `-inf`。

### Brain 模型结构

`Brain(version=4)` 使用 ResNet 编码观测。

默认配置来自 `mortal/config.example.toml`：

- `conv_channels = 192`
- `num_blocks = 40`
- 激活函数为 `Mish`
- 归一化为 `BatchNorm1d`
- 使用 pre-activation residual block

结构大致为：

1. 输入 `[B, C, 34]`。
2. `Conv1d(C, 192, kernel_size=3, padding=1, bias=false)`。
3. 40 个 residual block。
4. 每个 residual block 包含：
   - BatchNorm
   - Mish
   - Conv1d(192, 192, kernel_size=3, padding=1, bias=false)
   - BatchNorm
   - Mish
   - Conv1d(192, 192, kernel_size=3, padding=1, bias=false)
   - ChannelAttention
   - 残差相加
5. 尾部：
   - BatchNorm
   - Mish
   - Conv1d(192, 32, kernel_size=3, padding=1)
   - Mish
   - Flatten
   - Linear(32 * 34, 1024)
   - Mish

最终输出 `phi` 为 1024 维。

### ChannelAttention

每个 ResBlock 后带一个通道注意力模块：

- 对时间/牌种维做 mean pooling 和 max pooling。
- 两者共用一个两层 MLP：
  - Linear(192, 12)
  - ReLU
  - Linear(12, 192)
- mean 分支和 max 分支相加后 sigmoid，得到每个通道的权重。
- 用权重缩放卷积输出。

### DQN 输出头

`DQN(version=4)` 是一个简单线性头：

- `Linear(1024, 1 + ACTION_SPACE)`
- 输出拆成：
  - `v`：状态价值，1 维。
  - `a`：动作 advantage，46 维。

Q 值计算使用 dueling 结构：

```text
q = v + a - mean(a over legal actions)
```

随后非法动作通过 mask 置为 `-inf`。

### AuxNet

`AuxNet` 是一个线性层：

```text
Linear(1024, sum(dims), bias=false)
```

主训练中实例化为：

```text
AuxNet((4,))
```

即输出 4 维下一小局名次 logits，使用 cross entropy 训练。

### GRP 模型

GRP 用于估计半庄后续最终名次概率，不是主策略网络的一部分。

输入序列的每个时间步为 7 维：

```text
[grand_kyoku, honba, kyotaku, score0, score1, score2, score3]
```

其中分数按 `score / 10000` 编码。

默认结构：

- GRU input size = 7
- hidden size = 64
- num layers = 2
- FC：
  - Linear(128, 128)
  - ReLU
  - Linear(128, 24)

输出 24 维 logits，对应四家最终名次排列的 24 种 permutation。随后可汇总成 `[player, rank]` 的名次概率矩阵。

GRP 使用 float64 训练和推理。

### 参数量估算

当前环境无法导入 `torch` 直接实例化统计，因此以下为按代码结构和默认配置手算的近似值。

默认 `version=4`，`conv_channels=192`，`num_blocks=40`：

- Brain 初始 Conv1d：约 583K 参数。
- 每个 ResBlock 两个 192 通道 3x1 卷积：约 221K 参数。
- 40 个 ResBlock 卷积：约 8.85M 参数。
- 每个 ChannelAttention：约 4.8K 参数。
- 40 个 ChannelAttention：约 193K 参数。
- BatchNorm 参数：约 31K 参数。
- 尾部 Conv1d 和 Linear：约 1.16M 参数。
- Brain 合计约 10.8M 参数。
- DQN v4：`Linear(1024, 47)`，约 48K 参数。
- AuxNet((4,))：约 4K 参数。
- 主模型合计约 10.85M 参数。
- GRP 约 45K 参数。

因此 Mortal 默认主策略模型参数量大约是 11M 量级。

### 主训练目标

主训练数据以动作样本为单位，每个样本包括：

- `obs`
- `action`
- `mask`
- `steps_to_done`
- `kyoku_reward`
- `player_rank`

主 Q 目标：

```text
q_target = gamma ^ steps_to_done * kyoku_reward
```

`gamma` 默认配置为 1。

Q loss：

```text
0.5 * MSE(q(action), q_target)
```

离线训练时还加入 CQL 项：

```text
cql_loss = logsumexp(q_all_actions) - q(chosen_action)
```

总 loss：

```text
loss = dqn_loss + min_q_weight * cql_loss + next_rank_weight * next_rank_loss
```

默认配置：

- `min_q_weight = 5`
- `next_rank_weight = 0.2`

### 小局 reward 构造

Mortal 不直接把整场终局 pt 作为每个动作的唯一目标，而是用 GRP 构造小局级 delta pt。

流程：

1. 对半庄中每个小局开始前的 `[场次, 本场, 供托, 四家点数]` 序列，GRP 预测最终名次概率。
2. 将名次概率乘以 pt 分布，得到期望 pt。
3. 相邻小局开始前的期望 pt 相减，得到每个小局的 `kyoku_reward`。
4. 最后一段使用真实终局名次作为终点。

这和 CuKee 当前设计中的“小局开始前期望 pt baseline + 当前小局 delta”思路非常接近。

### 数据与训练流程

离线训练：

- 从 gzip JSON 牌谱文件加载。
- 可按玩家名过滤数据。
- 可启用数据增强。
- 从每个半庄中抽取每个动作点作为训练样本。
- 使用 `DataLoader` batch 训练。

在线训练：

- 支持训练进程和对局/采样进程分离。
- 训练进程定期提交参数。

优化配置默认：

- AdamW
- weight decay = 0.1
- betas = [0.9, 0.999]
- batch size = 512
- scheduler 为 linear warmup + cosine annealing，但示例配置中 warmup 和 max_steps 为 0，占位意味较强。
- 可选 AMP 和 `torch.compile`。

### 评估指标

训练中定期 self-play/test-play，记录：

- 平均名次
- 平均 pt
- 一二三四位率
- 和牌率
- 放铳率
- 副露率
- 立直率
- 平均和牌点
- 平均放铳点
- 每局平均点棒变化
- 和牌巡目、放铳巡目、立直巡目
- 立直后和牌/放铳
- 被追立/追立
- 副露后和牌/放铳

这些指标没有直接参与 loss，但用于评估模型风格和强度。

### 对 CuKee 的参考价值

Mortal 最值得参考的不是具体网络结构，而是以下设计点。

#### 小局 delta pt reward

Mortal 通过 GRP 估计小局开始前到终局的期望 pt，并用相邻小局的差值构造 reward。这和 CuKee 当前的“期望 pt 估计器 + 主模型预测当前小局 delta”高度一致，说明这个方向是可行且有先例的。

#### 进攻信息的低方差特征

Mortal 没有大量进攻辅助头，而是把进攻中间量编码进输入：

- shanten
- 保向听/进向听打牌
- 等待牌
- 单人 EV
- 有效牌
- 未来若干巡听牌概率
- 未来若干巡和牌概率
- 未来若干巡期望值

CuKee 如果不想直接把这些规则计算结果作为输入，也可以把它们作为辅助预测目标，帮助 RNN hidden 学习“速度”和“打点”。

#### 辅助任务很少但权重明确

Mortal 主训练里只有一个 next-rank 辅助任务，权重默认为 0.2。它没有让辅助任务主导训练。CuKee 如果有大量辅助头，需要按任务组归一化 loss，避免高维辅助任务压过主任务。

#### 动作 mask 与 dueling Q

Mortal 使用 dueling DQN，并在合法动作上计算 advantage 均值，再把非法动作置为 `-inf`。CuKee 如果输出每个合法决策的期望 pt，也应明确合法动作 mask 的位置和非法动作处理方式。

### 和 CuKee 当前设计的主要差异

- Mortal 是单状态 ResNet，不是事件序列 RNN。
- Mortal 依赖大量手工观测特征，CuKee 计划让 RNN 从事件序列中维护隐状态。
- Mortal 的主输出是动作 Q 值，CuKee 计划输出相对小局初 baseline 的期望 pt 变化。
- Mortal 的辅助任务很少，CuKee 计划使用大量防守和进攻辅助任务。
- Mortal 的进攻速度/EV 信息主要作为输入特征，CuKee 可考虑改造成辅助预测目标。

