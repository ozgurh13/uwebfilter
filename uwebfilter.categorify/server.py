
from train import train
from inference import Model

from fastapi import FastAPI, HTTPException, UploadFile, File, Form
from pydantic import BaseModel

from contextlib import suppress

import os
import pickle



app = FastAPI(
    title='Website Categorify',
    summary='Categorify Websites',
)




@app.get('/health')
def _health():
    '''
    health check
    '''
    return { 'success': True }



# prediction models, will be loaded as necessary
models = {}

class PayloadText(BaseModel):
    model: str
    text: str

@app.post('/inference')
def _inference(payload: PayloadText):
    '''
    given a model name a some text, categorize the text using the model
    '''
    model_name = payload.model
    model = models.get(model_name)
    if model is None:
        # try to load model file
        modelfile = f'models/{model_name}.pickle'
        if not os.path.isfile(modelfile):
            raise HTTPException(status_code=404, detail='model not found')

        model = Model.load(modelfile)
        models[model_name] = model

    prediction, probability = model.classify(payload.text)
    return { 'Prediction': prediction, **probability }




@app.post('/train', status_code=201)
def _train(name: str = Form(...), csvfile: UploadFile = File(...)):
    '''
    given a csv file with the form

        category,words
        Category1,word1 word2 word3 ...
        Category1,wordk wordk+1 wordk+2 ...
        Category1,wordn wordn+1 wordn+2 ...
        Category2,word1 word2 word3 ...
        Category2,wordk wordk+1 wordk+2 ...
        Category2,wordn wordn+1 wordn+2 ...
        ...

    train a model to predict the categories

    is a blocking endpoint, you will have to wait until the model has trained
    '''
    modelfile = f'models/{name}.pickle'
    if os.path.isfile(modelfile):
        raise HTTPException(status_code=409, detail="model already exists")

    utils = train(csvfile.file)
    with open(modelfile, 'wb') as handle:
        pickle.dump(utils, handle, protocol=pickle.HIGHEST_PROTOCOL)

    return { 'trained': name }




@app.get('/models')
def _models_get():
    '''
    return a list of trained models
    '''
    return [ model[:-7] for model in os.listdir('models') ]


class ModelDelete(BaseModel):
    name: str

@app.delete('/models')
def _models_delete(model: ModelDelete):
    '''
    delete the model with name `name`
    '''
    name = model.name
    modelfile = f'models/{name}.pickle'
    if not os.path.isfile(modelfile):
        raise HTTPException(status_code=404, detail='model not found')

    with suppress(FileNotFoundError):
        os.unlink(modelfile)

    return { 'deleted': name }




if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=9005)

