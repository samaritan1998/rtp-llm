import os
import torch
from typing import List, Dict, Any
from transformers import PreTrainedTokenizerBase

from maga_transformer.config.gpt_init_model_parameters import GptInitModelParameters
from maga_transformer.utils.util import to_torch_dtype
from maga_transformer.models.downstream_modules.custom_module import CustomModule, CustomHandler
from maga_transformer.config.gpt_init_model_parameters import GptInitModelParameters
from maga_transformer.models.downstream_modules.embedding.misc import combo_to_batch, EmbeddingRendererBase

from .api_datatype import EmbeddingResponseType

class ColBertEmbeddingModule(CustomModule):
    def __init__(self, config: GptInitModelParameters, tokenizer: PreTrainedTokenizerBase):
        super().__init__(config, tokenizer)
        self.renderer = ColbertEmbeddingRenderer(config, tokenizer)
        self.handler = ColBertEmbeddingHandler(config)


class ColbertEmbeddingRenderer(EmbeddingRendererBase):
    def __init__(self, config: GptInitModelParameters, tokenizer: PreTrainedTokenizerBase):
        super().__init__(config, tokenizer)
        self.embedding_type = EmbeddingResponseType.COLBERT
        
    def embedding_func(self, x: torch.Tensor) -> List[float]:
        assert isinstance(x, torch.Tensor)
        return x.tolist()
    
    def similar_func(self, left: torch.Tensor, right: torch.Tensor):
        assert isinstance(left, torch.Tensor) and isinstance(right, torch.Tensor), "colbert similaritey datatype error"
        token_scores = torch.einsum('in,jn->ij', left, right)
        scores, _ = token_scores.max(-1)
        scores = torch.sum(scores) / left.size(0)
        return float(scores)


class ColBertEmbeddingHandler(CustomHandler):
    def __init__(self, config: GptInitModelParameters):
        super().__init__(config)
        self.colbert_linear_path_ = os.path.join(self.config_.ckpt_path, 'colbert_linear.pt')
        if not os.path.exists(self.colbert_linear_path_):
            raise Exception('failed to find colbert_linear.pt from ckpt_path')
        self.dtype_ = to_torch_dtype(self.config_.data_type)

    def init(self, tensor_map: Dict[str, torch.Tensor]) -> None:
        sparse_linear_dict = torch.load(self.colbert_linear_path_, map_location='cpu')
        self.colbert_linear = torch.nn.Linear(in_features=self.config_.hidden_size, out_features=self.config_.hidden_size)
        self.colbert_linear.load_state_dict(sparse_linear_dict)
        self.colbert_linear = self.colbert_linear.to(self.dtype_).cuda()

    def _process_colbert_vecs(self, colbert_vecs: torch.Tensor, tokens_num: int):
        # delte the vectors of padding tokens
        return colbert_vecs[:tokens_num - 1]  # we don't use the embedding of cls, so select tokens_num-1

    def forward(self, input_ids: torch.Tensor, hidden_states: torch.Tensor, input_lengths: torch.Tensor, config: Dict[str, Any]):
        batch_input_ids, batch_hidden_states, batch_attention_mask = combo_to_batch(hidden_states, input_ids, input_lengths)
        colbert_vecs = self.colbert_linear(batch_hidden_states[:, 1:])
        colbert_vecs = colbert_vecs * batch_attention_mask[:, 1:][:, :, None].float()
        colbert_vecs = torch.nn.functional.normalize(colbert_vecs, dim=-1)
        all_colbert_vec = (list(map(self._process_colbert_vecs, colbert_vecs.cpu(), input_lengths)))
        return all_colbert_vec