/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <GemRepo/GemRepoScreen.h>
#include <GemRepo/GemRepoItemDelegate.h>
#include <GemRepo/GemRepoListView.h>
#include <GemRepo/GemRepoModel.h>
#include <GemRepo/GemRepoAddDialog.h>
#include <GemRepo/GemRepoInspector.h>
#include <PythonBindingsInterface.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <QMessageBox>
#include <QLabel>
#include <QHeaderView>
#include <QTableWidget>
#include <QFrame>
#include <QStackedWidget>

namespace O3DE::ProjectManager
{
    GemRepoScreen::GemRepoScreen(QWidget* parent)
        : ScreenWidget(parent)
    {
        m_gemRepoModel = new GemRepoModel(this);

        QVBoxLayout* vLayout = new QVBoxLayout();
        vLayout->setMargin(0);
        vLayout->setSpacing(0);
        setLayout(vLayout);

        m_contentStack = new QStackedWidget(this);

        m_noRepoContent = CreateNoReposContent();
        m_contentStack->addWidget(m_noRepoContent);

        m_repoContent = CreateReposContent();
        m_contentStack->addWidget(m_repoContent);

        vLayout->addWidget(m_contentStack);

        Reinit();
    }

    void GemRepoScreen::Reinit()
    {
        m_gemRepoModel->clear();
        FillModel();

        // If model contains any data show the repos
        if (m_gemRepoModel->rowCount())
        {
            m_contentStack->setCurrentWidget(m_repoContent);
        }
        else
        {
            m_contentStack->setCurrentWidget(m_noRepoContent);
        }

        // Select the first entry after everything got correctly sized
        QTimer::singleShot(200, [=]{
            QModelIndex firstModelIndex = m_gemRepoListView->model()->index(0,0);
            m_gemRepoListView->selectionModel()->select(firstModelIndex, QItemSelectionModel::ClearAndSelect);
        });
    }

    void GemRepoScreen::HandleAddRepoButton()
    {
        GemRepoAddDialog* repoAddDialog = new GemRepoAddDialog(this);

        if (repoAddDialog->exec() == QDialog::DialogCode::Accepted)
        {
            QString repoUrl = repoAddDialog->GetRepoPath();
            if (repoUrl.isEmpty())
            {
                return;
            }

            AZ::Outcome<void, AZStd::string> addGemRepoResult = PythonBindingsInterface::Get()->AddGemRepo(repoUrl);
            if (addGemRepoResult.IsSuccess())
            {
                Reinit();
            }
            else
            {
                QMessageBox::critical(this, tr("Operation failed"),
                    QString("Failed to add gem repo: %1.<br>Error:<br>%2").arg(repoUrl, addGemRepoResult.GetError().c_str()));
            }
        }
    }

    void GemRepoScreen::FillModel()
    {
        AZ::Outcome<QVector<GemRepoInfo>, AZStd::string> allGemRepoInfosResult = PythonBindingsInterface::Get()->GetAllGemRepoInfos();
        if (allGemRepoInfosResult.IsSuccess())
        {
            // Add all available repos to the model
            const QVector<GemRepoInfo> allGemRepoInfos = allGemRepoInfosResult.GetValue();
            for (const GemRepoInfo& gemRepoInfo : allGemRepoInfos)
            {
                m_gemRepoModel->AddGemRepo(gemRepoInfo);
            }
        }
        else
        {
            QMessageBox::critical(this, tr("Operation failed"), QString("Cannot retrieve gem repos for engine.<br>Error:<br>%2").arg(allGemRepoInfosResult.GetError().c_str()));
        }
    }

    QFrame* GemRepoScreen::CreateNoReposContent()
    {
        QFrame* contentFrame = new QFrame(this);

        QVBoxLayout* vLayout = new QVBoxLayout();
        vLayout->setAlignment(Qt::AlignHCenter);
        vLayout->setMargin(0);
        vLayout->setSpacing(0);
        contentFrame->setLayout(vLayout);

        vLayout->addStretch();

        QLabel* noRepoLabel = new QLabel(tr("No repositories have been added yet."), this);
        noRepoLabel->setObjectName("gemRepoNoReposLabel");
        vLayout->addWidget(noRepoLabel);
        vLayout->setAlignment(noRepoLabel, Qt::AlignHCenter);

        vLayout->addSpacing(20);

        // Size hint for button is wrong so horizontal layout with stretch is used to center it
        QHBoxLayout* hLayout = new QHBoxLayout();
        hLayout->setMargin(0);
        hLayout->setSpacing(0);

        hLayout->addStretch();

        QPushButton* addRepoButton = new QPushButton(tr("Add Repository"), this);
        addRepoButton->setObjectName("gemRepoAddButton");
        addRepoButton->setMinimumWidth(120);
        hLayout->addWidget(addRepoButton);

        connect(addRepoButton, &QPushButton::clicked, this, &GemRepoScreen::HandleAddRepoButton);

        hLayout->addStretch();

        vLayout->addLayout(hLayout);

        vLayout->addStretch();

        return contentFrame;
    }

    QFrame* GemRepoScreen::CreateReposContent()
    {
        QFrame* contentFrame = new QFrame(this);

        QHBoxLayout* hLayout = new QHBoxLayout();
        hLayout->setMargin(0);
        hLayout->setSpacing(0);
        contentFrame->setLayout(hLayout);

        hLayout->addSpacing(60);

        QVBoxLayout* middleVLayout = new QVBoxLayout();
        middleVLayout->setMargin(0);
        middleVLayout->setSpacing(0);

        middleVLayout->addSpacing(30);

        QHBoxLayout* topMiddleHLayout = new QHBoxLayout();
        topMiddleHLayout->setMargin(0);
        topMiddleHLayout->setSpacing(0);

        m_lastAllUpdateLabel = new QLabel(tr("Last Updated: Never"), this);
        m_lastAllUpdateLabel->setObjectName("gemRepoHeaderLabel");
        topMiddleHLayout->addWidget(m_lastAllUpdateLabel);

        topMiddleHLayout->addSpacing(20);

        m_AllUpdateButton = new QPushButton(QIcon(":/Refresh.svg"), tr("Update All"), this);
        m_AllUpdateButton->setObjectName("gemRepoHeaderRefreshButton");
        topMiddleHLayout->addWidget(m_AllUpdateButton);

        topMiddleHLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

        QPushButton* addRepoButton = new QPushButton(tr("Add Repository"), this);
        addRepoButton->setObjectName("gemRepoAddButton");
        topMiddleHLayout->addWidget(addRepoButton);

        connect(addRepoButton, &QPushButton::clicked, this, &GemRepoScreen::HandleAddRepoButton);

        topMiddleHLayout->addSpacing(30);

        middleVLayout->addLayout(topMiddleHLayout);

        middleVLayout->addSpacing(30);

        // Create a QTableWidget just for its header
        // Using a seperate model allows the setup  of a header exactly as needed
        m_gemRepoHeaderTable = new QTableWidget(this);
        m_gemRepoHeaderTable->setObjectName("gemRepoHeaderTable");
        m_gemRepoListHeader = m_gemRepoHeaderTable->horizontalHeader();
        m_gemRepoListHeader->setObjectName("gemRepoListHeader");
        m_gemRepoListHeader->setSectionResizeMode(QHeaderView::ResizeMode::Fixed);

        // Insert columns so the header labels will show up
        m_gemRepoHeaderTable->insertColumn(0);
        m_gemRepoHeaderTable->insertColumn(1);
        m_gemRepoHeaderTable->insertColumn(2);
        m_gemRepoHeaderTable->insertColumn(3);
        m_gemRepoHeaderTable->setHorizontalHeaderLabels({ tr("Enabled"), tr("Repository Name"), tr("Creator"), tr("Updated") });

        const int headerExtraMargin = 10;
        m_gemRepoListHeader->resizeSection(0, GemRepoItemDelegate::s_buttonWidth + GemRepoItemDelegate::s_buttonSpacing - 3);
        m_gemRepoListHeader->resizeSection(1, GemRepoItemDelegate::s_nameMaxWidth + GemRepoItemDelegate::s_contentSpacing - headerExtraMargin);
        m_gemRepoListHeader->resizeSection(2, GemRepoItemDelegate::s_creatorMaxWidth + GemRepoItemDelegate::s_contentSpacing - headerExtraMargin);
        m_gemRepoListHeader->resizeSection(3, GemRepoItemDelegate::s_updatedMaxWidth + GemRepoItemDelegate::s_contentSpacing - headerExtraMargin);

        // Required to set stylesheet in code as it will not be respected if set in qss
        m_gemRepoHeaderTable->horizontalHeader()->setStyleSheet("QHeaderView::section { background-color:transparent; color:white; font-size:12px; text-align:left; border-style:none; }");
        middleVLayout->addWidget(m_gemRepoHeaderTable);

        m_gemRepoListView = new GemRepoListView(m_gemRepoModel, m_gemRepoModel->GetSelectionModel(), this);
        middleVLayout->addWidget(m_gemRepoListView);

        hLayout->addLayout(middleVLayout);

        m_gemRepoInspector = new GemRepoInspector(m_gemRepoModel, this);
        m_gemRepoInspector->setFixedWidth(240);
        hLayout->addWidget(m_gemRepoInspector);

        return contentFrame;
    }

    ProjectManagerScreen GemRepoScreen::GetScreenEnum()
    {
        return ProjectManagerScreen::GemRepos;
    }
} // namespace O3DE::ProjectManager
